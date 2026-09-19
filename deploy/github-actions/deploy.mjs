import { createReadStream } from "node:fs";
import { rm } from "node:fs/promises";
import path from "node:path";
import { spawn, spawnSync } from "node:child_process";
import { pathToFileURL } from "node:url";

const EXPECTED_CORE_SHA = "54308d8473dc5606d054c0ba428cfb2d64e758c1";
const SHA_PATTERN = /^[0-9a-f]{40}$/;
const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/;
const TARGET_PATTERN = /^[a-z_][a-z0-9_-]*@[a-z0-9.-]+$/i;
const BLOCK_LINK_PATTERN = /\/block\/([0-9a-f]{64})/;

export function parseTargets(value) {
    const targets = String(value || "")
        .split(/\r?\n/)
        .map((entry) => entry.trim())
        .filter(Boolean);
    if (targets.length !== 4 || new Set(targets).size !== targets.length) {
        throw new Error("The production inventory must contain four unique targets");
    }
    if (!targets.every((target) => TARGET_PATTERN.test(target))) {
        throw new Error("The production inventory contains a malformed target");
    }
    return targets;
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
    if (result.status !== 0) {
        throw new Error(`${commandName} failed`);
    }
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
            else reject(new Error("Remote deployment command failed"));
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
        headers: {"accept": "application/json"},
        signal: AbortSignal.timeout(20_000)
    });
    if (!response.ok) throw new Error(`Public API route ${route} failed`);
    return await response.json();
}

async function verifyPublic(origin, explorerSha) {
    const parsedOrigin = new URL(origin);
    if (parsedOrigin.protocol !== "https:" || parsedOrigin.username
            || parsedOrigin.password || parsedOrigin.pathname !== "/") {
        throw new Error("The public Explorer origin is malformed");
    }

    const version = await getJson(parsedOrigin, "/api/v1/version");
    if (version?.status !== "success"
            || version?.data?.explorer_source_sha !== explorerSha
            || version?.data?.qwc_source_sha !== EXPECTED_CORE_SHA) {
        throw new Error("The public Explorer reports the wrong source pair");
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
    if (!coherent) throw new Error("The public EPoSe snapshot is incoherent");

    const pages = ["/", "/epochs", "/service-nodes", "/blocks", "/blocks/1"];
    const bodies = [];
    for (const route of pages) {
        const response = await fetch(new URL(route, parsedOrigin), {
            signal: AbortSignal.timeout(20_000)
        });
        if (!response.ok) throw new Error(`Public page ${route} failed`);
        bodies.push(await response.text());
    }
    for (const body of bodies.slice(0, 3)) {
        if (!body.includes("Qualification") || !body.includes("Reward eligibility")
                || body.includes("qualified: no")) {
            throw new Error("The public qualification UI is stale");
        }
    }
    const firstBlock = bodies[3].match(BLOCK_LINK_PATTERN)?.[1];
    const secondPageBlock = bodies[4].match(BLOCK_LINK_PATTERN)?.[1];
    if (!firstBlock || !secondPageBlock || firstBlock === secondPageBlock) {
        throw new Error("The public block pagination is stale");
    }
}

async function main() {
    const targets = parseTargets(process.env.QWC_DEPLOY_TARGETS);
    const imageTag = process.env.QWC_DEPLOY_IMAGE_TAG;
    const explorerSha = process.env.QWC_DEPLOY_EXPLORER_SHA;
    const imageDigest = process.env.QWC_DEPLOY_IMAGE_DIGEST;
    const publicOrigin = process.env.QWC_EXPLORER_PUBLIC_ORIGIN;
    if (!SHA_PATTERN.test(explorerSha || "") || !DIGEST_PATTERN.test(imageDigest || "")) {
        throw new Error("The immutable deployment identity is malformed");
    }
    if (imageTag !== `qwertycoin-explorer:deploy-${explorerSha}`) {
        throw new Error("The local deployment tag is not commit-bound");
    }

    const inspected = JSON.parse(command("docker", ["image", "inspect", imageTag]));
    if (inspected.length !== 1
            || inspected[0]?.Config?.Labels?.["org.opencontainers.image.revision"] !== explorerSha
            || inspected[0]?.Config?.Labels?.["org.qwertycoin.core.revision"] !== EXPECTED_CORE_SHA
            || inspected[0]?.Architecture !== "amd64"
            || inspected[0]?.Os !== "linux") {
        throw new Error("The pulled image does not match the reviewed source pair");
    }

    const archive = path.join(process.env.RUNNER_TEMP, `qwc-explorer-${explorerSha}.tar`);
    command("docker", ["save", "--output", archive, imageTag]);
    try {
        for (let index = 0; index < targets.length; ++index) {
            const loaded = await ssh(targets[index], "image-load", archive);
            if (!/\bIMAGE_OK\b/.test(loaded)) {
                throw new Error(`Target ${index + 1} rejected the image`);
            }
            const deployed = await ssh(targets[index], `rollout ${explorerSha}`);
            if (!/\b(?:ROLLOUT_OK|ALREADY_CURRENT)\b/.test(deployed)) {
                throw new Error(`Target ${index + 1} failed post-deployment validation`);
            }
            console.log(`Production target ${index + 1}/${targets.length}: verified`);
        }
        await verifyPublic(publicOrigin, explorerSha);
        console.log("Public Explorer verification: verified");
    } finally {
        await rm(archive, {force: true});
    }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
    await main();
}
