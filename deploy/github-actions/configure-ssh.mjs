import { chmod, mkdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

const privateKey = process.env.QWC_DEPLOY_SSH_PRIVATE_KEY;
const knownHosts = process.env.QWC_DEPLOY_SSH_KNOWN_HOSTS;
const runnerTemp = process.env.RUNNER_TEMP;

if (!privateKey || !privateKey.includes("BEGIN OPENSSH PRIVATE KEY")) {
    throw new Error("The production deployment key is missing or malformed");
}
if (!knownHosts || knownHosts.trim().length === 0) {
    throw new Error("The pinned production host-key inventory is missing");
}
if (!runnerTemp || !path.isAbsolute(runnerTemp)) {
    throw new Error("RUNNER_TEMP is unavailable");
}

const sshDirectory = path.join(runnerTemp, "qwc-explorer-deploy-ssh");
await mkdir(sshDirectory, {recursive: true, mode: 0o700});
await chmod(sshDirectory, 0o700);
await writeFile(path.join(sshDirectory, "id_ed25519"), `${privateKey.trim()}\n`, {
    encoding: "utf8",
    mode: 0o600
});
await writeFile(path.join(sshDirectory, "known_hosts"), `${knownHosts.trim()}\n`, {
    encoding: "utf8",
    mode: 0o600
});
