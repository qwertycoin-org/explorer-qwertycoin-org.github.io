import { rm } from "node:fs/promises";
import path from "node:path";

const runnerTemp = process.env.RUNNER_TEMP;
if (runnerTemp && path.isAbsolute(runnerTemp)) {
    await rm(path.join(runnerTemp, "qwc-explorer-deploy-ssh"), {
        recursive: true,
        force: true
    });
}
