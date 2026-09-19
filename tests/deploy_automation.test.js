"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { pathToFileURL } = require("node:url");

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
    const integrationDeploy = await import(pathToFileURL(path.resolve(
        __dirname, "../deploy/github-actions/integration-deploy.mjs")));

    assert.deepEqual(deploy.parseTargets([
        "deploy@canary.invalid",
        "deploy@second.invalid",
        "deploy@third.invalid",
        "deploy@fourth.invalid"
    ].join("\n")), [
        "deploy@canary.invalid",
        "deploy@second.invalid",
        "deploy@third.invalid",
        "deploy@fourth.invalid"
    ]);
    assert.throws(() => deploy.parseTargets("deploy@only.invalid"));
    assert.throws(() => deploy.parseTargets([
        "deploy@same.invalid", "deploy@same.invalid",
        "deploy@third.invalid", "deploy@fourth.invalid"
    ].join("\n")));
    assert.throws(() => deploy.parseTargets([
        "deploy@one.invalid", "deploy@two.invalid",
        "deploy@three.invalid", "deploy@four.invalid;touch /tmp/bad"
    ].join("\n")));
    assert.equal(integrationDeploy.parseIntegrationTarget("deploy@integration.invalid"),
        "deploy@integration.invalid");
    assert.throws(() => integrationDeploy.parseIntegrationTarget(""));
    assert.throws(() => integrationDeploy.parseIntegrationTarget(
        "deploy@integration.invalid\ndeploy@production.invalid"));
    assert.throws(() => integrationDeploy.parseIntegrationTarget(
        "deploy@integration.invalid;touch /tmp/bad"));

    const info = anchor(2880);
    info.data.current_epoch = 4;
    const nodes = anchor(2880);
    nodes.data.current_epoch = 4;
    nodes.data.source_epoch = 3;
    const rewards = anchor(2880);
    rewards.data.height = 2880;
    rewards.data.epoch = 3;
    assert.equal(deploy.sameSnapshot(info, nodes, rewards), true);
    assert.equal(integrationDeploy.sameSnapshot(info, nodes, rewards), true);
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
        "deploy/github-actions/remote-gate.sh",
        ".github/workflows/integration.yml",
        "deploy/github-actions/integration-deploy.mjs",
        "deploy/github-actions/integration-remote-gate.sh"
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

    const integrationWorkflow = fs.readFileSync(path.resolve(
        __dirname, "../.github/workflows/integration.yml"), "utf8");
    assert.match(integrationWorkflow, /branches: \[integration\]/);
    assert.match(integrationWorkflow, /environment: integration/);
    assert.match(integrationWorkflow, /secrets\.QWC_INTEGRATION_DEPLOY_TARGET/);
    assert.match(integrationWorkflow, /vars\.QWC_INTEGRATION_PUBLIC_ORIGIN/);
    assert.doesNotMatch(integrationWorkflow, /refs\/heads\/master/);

    const remoteGate = fs.readFileSync(path.resolve(
        __dirname, "../deploy/github-actions/remote-gate.sh"), "utf8");
    assert.match(remoteGate, /docker network disconnect/,
        "the stopped rollback container must release its static address");
    assert.match(remoteGate, /docker network connect --ip/,
        "a failed rollout must restore the previous network endpoint");

    const integrationGate = fs.readFileSync(path.resolve(
        __dirname, "../deploy/github-actions/integration-remote-gate.sh"), "utf8");
    assert.match(integrationGate, /role_value='integration-explorer'/);
    assert.match(integrationGate, /org\.qwertycoin\.role=\$\{role_value\}/);
    assert.match(integrationGate, /INTEGRATION_DERIVED_VOLUME/);
    assert.doesNotMatch(integrationGate, /docker (?:stop|rename).*production/,
        "the integration gate must never mutate the production container");
    assert.doesNotMatch(integrationGate, /production_core/,
        "a Core-pin candidate must be testable before production uses that pin");
    assert.equal((integrationGate.match(/== "\$\{expected_core_sha\}"/g) || []).length, 1,
        "only the integration candidate may be required to use the new Core pin");

    console.log("Deployment automation tests passed");
})().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});
