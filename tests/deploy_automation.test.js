"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
const { spawnSync } = require("node:child_process");

function anchor(blockCount, hashCharacter = "a") {
    return {
        status: "success",
        data: {
            snapshot_block_count: blockCount,
            snapshot_tip_height: blockCount - 1,
            snapshot_tip_hash: hashCharacter.repeat(64),
            snapshot_consistency: "anchored"
        }
    };
}

(async () => {
    const deploy = await import(pathToFileURL(path.resolve(
        __dirname, "../deploy/github-actions/deploy.mjs")));

    assert.deepEqual(deploy.parseTargets("deploy@production.invalid"), [
        "deploy@production.invalid"
    ]);
    assert.throws(() => deploy.parseTargets(""));
    assert.throws(() => deploy.parseTargets([
        "deploy@first.invalid", "deploy@second.invalid"
    ].join("\n")));
    assert.throws(() => deploy.parseTargets(
        "deploy@production.invalid;touch /tmp/bad"));

    const info = anchor(2880);
    info.data.current_epoch = 4;
    const nodes = anchor(2880);
    nodes.data.current_epoch = 4;
    nodes.data.source_epoch = 3;
    const rewards = anchor(2880);
    rewards.data.height = 2880;
    rewards.data.epoch = 3;
    assert.equal(deploy.sameSnapshot(info, nodes, rewards), true);
    assert.equal(deploy.sameSnapshot(info, anchor(2879), rewards), false);
    const reorged = anchor(2880, "b");
    reorged.data.current_epoch = 4;
    reorged.data.source_epoch = 3;
    assert.equal(deploy.sameSnapshot(info, reorged, rewards), false);
    const wrongRewardEpoch = structuredClone(rewards);
    wrongRewardEpoch.data.epoch = 2;
    assert.equal(deploy.sameSnapshot(info, nodes, wrongRewardEpoch), false);

    const publicFiles = [
        ".github/workflows/ci.yml",
        "deploy/github-actions/configure-ssh.mjs",
        "deploy/github-actions/cleanup-ssh.mjs",
        "deploy/github-actions/deploy.mjs",
        "deploy/github-actions/remote-gate.sh"
    ].map((name) => fs.readFileSync(path.resolve(__dirname, "..", name), "utf8"))
        .join("\n");
    const literalAddresses = publicFiles.match(/\b(?:\d{1,3}\.){3}\d{1,3}\b/g) || [];
    assert.deepEqual([...new Set(literalAddresses)], ["127.0.0.1"],
        "deployment automation may contain only the loopback invariant");
    assert.doesNotMatch(publicFiles, /StrictHostKeyChecking\s*=\s*no/i);
    assert.doesNotMatch(publicFiles, /ssh-keyscan/i);
    assert.doesNotMatch(publicFiles, /seed-[0-9]+/i);

    const workflow = fs.readFileSync(path.resolve(
        __dirname, "../.github/workflows/ci.yml"), "utf8");
    assert.match(workflow, /environment: production/);
    assert.match(workflow, /secrets\.QWC_DEPLOY_SSH_PRIVATE_KEY/);
    assert.match(workflow, /secrets\.QWC_DEPLOY_SSH_KNOWN_HOSTS/);
    assert.match(workflow, /secrets\.QWC_DEPLOY_TARGETS/);
    assert.match(workflow, /needs\.supported-container-build\.outputs\.digest/);
    const nonActionLines = workflow.split("\n")
        .filter((line) => !/^\s*-?\s*uses:/.test(line))
        .join("\n");
    assert.doesNotMatch(nonActionLines, /\b[a-z_][a-z0-9_-]*@[a-z0-9.-]+\b/i);

    const remoteGate = fs.readFileSync(path.resolve(
        __dirname, "../deploy/github-actions/remote-gate.sh"), "utf8");
    assert.match(remoteGate, /docker network disconnect/,
        "the stopped rollback container must release its static address");
    assert.match(remoteGate, /docker network connect --ip/,
        "a failed rollout must restore the previous network endpoint");
    assert.doesNotMatch(remoteGate, /current_core_revision/,
        "a Core-pin candidate must be deployable before production uses that pin");
    assert.match(remoteGate, /core_revision=\$\(docker image inspect/,
        "the candidate image must still expose a validated Core revision");

    const gatePath = path.resolve(__dirname, "../deploy/github-actions/remote-gate.sh");
    for (const [memory, swap, accepted] of [
        ["2147483648", "4294967296", true],
        ["2952790016", "4294967296", true],
        ["2952790016", "2952790016", true],
        ["0", "4294967296", false],
        ["2952790016", "0", false],
        ["2952790016", "-1", false],
        ["2952790016", "2147483648", false],
        ["02952790016", "4294967296", false],
        ["9223372036854775808", "9223372036854775808", false],
        ["1+1", "4294967296", false],
        ["", "4294967296", false]
    ]) {
        const check = spawnSync("bash", ["-c",
            'source "$1"; valid_memory_limits "$2" "$3"',
            "memory-limit-test", gatePath, memory, swap]);
        assert.equal(check.status, accepted ? 0 : 1,
            `memory/swap limits ${memory}/${swap} must ${accepted ? "pass" : "fail closed"}`);
    }
    assert.match(remoteGate, /--memory "\$\{memory_limit\}"/);
    assert.match(remoteGate, /--memory-swap "\$\{memory_swap_limit\}"/);
    assert.doesNotMatch(remoteGate, /--memory 2g/,
        "a deployment must not silently replace the active memory budget");

    console.log("Deployment automation tests passed");
})().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});
