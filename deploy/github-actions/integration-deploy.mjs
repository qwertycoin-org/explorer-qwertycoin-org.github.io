import {createReadStream} from "node:fs";
import {rm} from "node:fs/promises";
import path from "node:path";
import {spawn, spawnSync} from "node:child_process";
import {pathToFileURL} from "node:url";

const EXPECTED_CORE_SHA = "54308d8473dc5606d054c0ba428cfb2d64e758c1";
const SHA_PATTERN = /^[0-9a-f]{40}$/;
const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/;
const TARGET_PATTERN = /^[a-z_][a-z0-9_-]*@[a-z0-9.-]+$/i;

export function parseIntegrationTarget(value) {
    const target = String(value || "").trim();
    if (!TARGET_PATTERN.test(target) || target.includes("\n") || target.includes("\r")) {
        throw new Error("The integration target is malformed");
    }
    return target;
}

export function sameSnapshot(info, nodes, rewards) {
    const sections = [info, nodes, rewards];
    if (!sections.every((section) => section?.status === "success"
            && section?.data?.snapshot_consistency === "anchored")) {
        return false;
    }
    const expected = info.data;
    return nodes.data.snapshot_block_count === expected.snapshot_block_count
        && rewards.data.snapshot_block_count === expected.snapshot_block_count
        && nodes.data.snapshot_tip_height === expected.snapshot_tip_height
        && rewards.data.snapshot_tip_height === expected.snapshot_tip_height
        && nodes.data.snapshot_tip_hash === expected.snapshot_tip_hash
        && rewards.data.snapshot_tip_hash === expected.snapshot_tip_hash
        && rewards.data.height === expected.snapshot_block_count
        && nodes.data.current_epoch === expected.current_epoch
        && nodes.data.source_epoch === rewards.data.epoch
        && rewards.data.epoch + 1 === expected.current_epoch;
}

function command(commandName, args, options = {}) {
    const result = spawnSync(commandName, args, {
        encoding: "utf8",
        maxBuffer: 1024 * 1024,
        ...options
    });
    if (result.status !== 0) throw new Error(`${commandName} failed`);
    return result.stdout;
}

async function ssh(target, remoteCommand, inputPath = null) {
    const sshDirectory = path.join(process.env.RUNNER_TEMP, "qwc-explorer-deploy-ssh");
    const args = [
        "-o", "BatchMode=yes",
        "-o", "IdentitiesOnly=yes",
        "-o", "StrictHostKeyChecking=yes",
        "-o", `UserKnownHostsFile=${path.join(sshDirectory, "known_hosts")}`,
        "-o", "ConnectTimeout=15",
        "-o", "LogLevel=ERROR",
        "-i", path.join(sshDirectory, "id_ed25519"),
        target,
        remoteCommand
    ];
    return await new Promise((resolve, reject) => {
        const child = spawn("ssh", args, {stdio: ["pipe", "pipe", "pipe"]});
        let output = "";
        const collect = (chunk) => {
            if (output.length < 128 * 1024) output += chunk.toString("utf8");
        };
        child.stdout.on("data", collect);
        child.stderr.on("data", collect);
        child.once("error", () => reject(new Error("SSH transport failed")));
        child.once("close", (code) => {
            if (code === 0) resolve(output);
            else reject(new Error("Remote integration deployment command failed"));
        });
        if (inputPath) {
            const input = createReadStream(inputPath);
            input.once("error", () => child.kill("SIGTERM"));
            input.pipe(child.stdin);
        } else {
            child.stdin.end();
        }
        setTimeout(() => child.kill("SIGTERM"), 10 * 60 * 1000).unref();
    });
}

async function getJson(origin, route) {
    const response = await fetch(new URL(route, origin), {
        headers: {accept: "application/json"},
        signal: AbortSignal.timeout(20_000)
    });
    if (!response.ok) throw new Error(`Public integration API route ${route} failed`);
    return await response.json();
}

async function verifyPublic(origin, explorerSha) {
    const parsedOrigin = new URL(origin);
    if (parsedOrigin.protocol !== "https:" || parsedOrigin.username
            || parsedOrigin.password || parsedOrigin.pathname !== "/") {
        throw new Error("The public integration origin is malformed");
    }
    const version = await getJson(parsedOrigin, "/api/v1/version");
    if (version?.status !== "success"
            || version?.data?.explorer_source_sha !== explorerSha
            || version?.data?.qwc_source_sha !== EXPECTED_CORE_SHA) {
        throw new Error("The public integration Explorer reports the wrong source pair");
    }

    let coherent = false;
    for (let attempt = 0; attempt < 10 && !coherent; ++attempt) {
        const [info, nodes, rewards] = await Promise.all([
            getJson(parsedOrigin, "/api/v1/epose"),
            getJson(parsedOrigin, "/api/v1/epose/service-nodes"),
            getJson(parsedOrigin, "/api/v1/epose/rewards")
        ]);
        coherent = sameSnapshot(info, nodes, rewards);
        if (!coherent) await new Promise((resolve) => setTimeout(resolve, 1_000));
    }
    if (!coherent) throw new Error("The public integration EPoSe snapshot is incoherent");

    for (const route of ["/", "/epochs", "/service-nodes", "/blocks", "/blocks/1"]) {
        const response = await fetch(new URL(route, parsedOrigin), {
            signal: AbortSignal.timeout(20_000)
        });
        if (!response.ok) throw new Error(`Public integration page ${route} failed`);
    }
}

async function main() {
    const target = parseIntegrationTarget(process.env.QWC_INTEGRATION_DEPLOY_TARGET);
    const imageTag = process.env.QWC_DEPLOY_IMAGE_TAG;
    const explorerSha = process.env.QWC_DEPLOY_EXPLORER_SHA;
    const imageDigest = process.env.QWC_DEPLOY_IMAGE_DIGEST;
    const publicOrigin = process.env.QWC_INTEGRATION_PUBLIC_ORIGIN;
    if (!SHA_PATTERN.test(explorerSha || "") || !DIGEST_PATTERN.test(imageDigest || "")) {
        throw new Error("The immutable integration deployment identity is malformed");
    }
    if (imageTag !== `qwertycoin-explorer:integration-deploy-${explorerSha}`) {
        throw new Error("The local integration deployment tag is not commit-bound");
    }

    const inspected = JSON.parse(command("docker", ["image", "inspect", imageTag]));
    if (inspected.length !== 1
            || inspected[0]?.Config?.Labels?.["org.opencontainers.image.revision"] !== explorerSha
            || inspected[0]?.Config?.Labels?.["org.qwertycoin.core.revision"] !== EXPECTED_CORE_SHA
            || inspected[0]?.Architecture !== "amd64"
            || inspected[0]?.Os !== "linux") {
        throw new Error("The integration image does not match the reviewed source pair");
    }

    const archive = path.join(process.env.RUNNER_TEMP, `qwc-explorer-integration-${explorerSha}.tar`);
    command("docker", ["save", "--output", archive, imageTag]);
    try {
        const loaded = await ssh(target, "image-load", archive);
        if (!/\bIMAGE_OK\b/.test(loaded)) throw new Error("Integration target rejected the image");
        const deployed = await ssh(target, `rollout ${explorerSha}`);
        if (!/\b(?:ROLLOUT_OK|ALREADY_CURRENT)\b/.test(deployed)) {
            throw new Error("Integration target failed post-deployment validation");
        }
        console.log("Integration target: verified");
        await verifyPublic(publicOrigin, explorerSha);
        console.log("Public integration Explorer: verified");
    } finally {
        await rm(archive, {force: true});
    }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
    await main();
}
