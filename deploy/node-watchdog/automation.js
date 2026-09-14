const watchdogPath = "/workspace/ops/qwc-node-watchdog/watchdog.sh";
const expectedWatchdogSha256 = "1f5aceb28c374671b9159f256677cb44bf6a77c37a9982e96aea4fbbe062555f";
const verifyWatchdog = `printf '${expectedWatchdogSha256}  ${watchdogPath}\\n' | sha256sum --check --status`;
const probeCommand = `${verifyWatchdog} && ${watchdogPath} probe`;
const repairCommand = `${verifyWatchdog} && ${watchdogPath} repair`;
const now = Date.now();
const confirmAfter = 3;
const actionCooldownMs = 15 * 60 * 1000;
const alertCooldownMs = 60 * 60 * 1000;

function commandOutput(result) {
  return String(result?.aggregated || result?.stdout || result?.output || "").trim();
}

async function runCommand(command, timeoutMs) {
  let result = await exec({command, timeoutMs});
  let polls = 0;
  while (result?.status === "running" && polls < 10) {
    result = await process({
      action: "poll",
      sessionId: result.sessionId,
      timeout: Math.min(timeoutMs, 30000),
    });
    polls += 1;
  }
  if (result?.status === "running") {
    throw new Error(`command did not finish after ${polls} polls`);
  }
  if (Number(result?.exitCode ?? 0) !== 0) {
    throw new Error(`command exited with ${result.exitCode}: ${commandOutput(result)}`);
  }
  return result;
}

function safeState(previous) {
  return {
    counts: previous?.counts && typeof previous.counts === "object" ? {...previous.counts} : {},
    lastFired: previous?.lastFired && typeof previous.lastFired === "object" ? {...previous.lastFired} : {},
    probeErrors: Number(previous?.probeErrors ?? 0),
    lastProbeErrorAlertAt: Number(previous?.lastProbeErrorAlertAt ?? 0),
    lastCheckedAt: Number(previous?.lastCheckedAt ?? 0),
    lastHealthyAt: Number(previous?.lastHealthyAt ?? 0),
  };
}

let state = safeState(trigger.state);
let snapshot;

try {
  const probeResult = await runCommand(probeCommand, 30000);
  const output = commandOutput(probeResult);
  snapshot = JSON.parse(output);
  state.probeErrors = 0;
} catch (error) {
  state.probeErrors += 1;
  state.lastCheckedAt = now;
  const due = state.probeErrors >= 2 && now - state.lastProbeErrorAlertAt >= alertCooldownMs;
  if (!due) return {state};
  state.lastProbeErrorAlertAt = now;
  return {
    notify: `QWC-Watchdog-Alarm: Die zentrale Prüfung selbst ist ${state.probeErrors} Mal in Folge fehlgeschlagen. Es wurde kein Container verändert. Fehler: ${String(error)}`,
    state,
  };
}

state.lastCheckedAt = now;

if (snapshot.paused === true) {
  state.counts = {};
  return {state};
}

const faults = Array.isArray(snapshot.faults) ? snapshot.faults : [];
if (faults.length === 0) {
  state.counts = {};
  state.lastHealthyAt = now;
  return {state};
}

const activeKeys = new Set(faults.map((fault) => String(fault.key)));
for (const key of Object.keys(state.counts)) {
  if (!activeKeys.has(key)) delete state.counts[key];
}
for (const fault of faults) {
  const key = String(fault.key);
  state.counts[key] = Number(state.counts[key] ?? 0) + 1;
}

const severityRank = {invariant: 0, service: 1, alert: 2};
const ordered = [...faults].sort((left, right) => {
  const severity = (severityRank[left.severity] ?? 9) - (severityRank[right.severity] ?? 9);
  if (severity !== 0) return severity;
  return String(left.key).localeCompare(String(right.key));
});

const selected = ordered.find((fault) => {
  const key = String(fault.key);
  const threshold = fault.severity === "invariant" ? 1 : confirmAfter;
  const cooldown = fault.severity === "service" ? actionCooldownMs : alertCooldownMs;
  return state.counts[key] >= threshold && now - Number(state.lastFired[key] ?? 0) >= cooldown;
});

if (!selected) return {state};

const selectedGroup = selected.severity === "service"
  ? [selected]
  : ordered.filter((fault) =>
      fault.severity === selected.severity &&
      fault.role === selected.role &&
      fault.reason === selected.reason);
for (const fault of selectedGroup) {
  const groupedKey = String(fault.key);
  state.lastFired[groupedKey] = now;
  state.counts[groupedKey] = 0;
}

const host = selectedGroup.map((fault) => String(fault.host ?? "")).join(", ");
const singleHost = String(selected.host ?? "");
const role = String(selected.role ?? "");
const reason = String(selected.reason ?? "unknown");
const safeTarget = /^seed-0[0-3]\.qwertycoin\.org$/.test(singleHost) && /^(core|explorer|gateway)$/.test(role);

if (selected.severity !== "service" || !safeTarget) {
  return {
    notify: `QWC-Watchdog-Alarm: ${host}/${role} meldet ${reason}. Dieser Fehler ist absichtlich nur meldepflichtig; es wurde nichts neu gestartet.`,
    state,
  };
}

try {
  const repairResult = await runCommand(`${repairCommand} ${singleHost} ${role}`, 240000);
  const repairOutput = commandOutput(repairResult);
  const repair = JSON.parse(repairOutput);
  const status = String(repair.status ?? "unknown");

  if (status === "repaired") {
    return {
      notify: `QWC-Watchdog: ${singleHost}/${role} wurde nach drei bestätigten Fehlern per ${repair.action} wiederhergestellt und nach ${repair.verified_after_seconds} Sekunden funktional verifiziert. Ursache: ${repair.reason}.`,
      state,
    };
  }
  if (status === "recovered_without_action") {
    return {
      notify: `QWC-Watchdog: ${singleHost}/${role} war beim Reparaturlauf bereits wieder gesund. Es wurde nichts neu gestartet.`,
      state,
    };
  }

  return {
    notify: `QWC-Watchdog-Alarm: ${singleHost}/${role} blieb fehlerhaft, wurde aber nicht automatisch verändert. Reparaturstatus: ${status}; Grund: ${repair.reason ?? reason}.`,
    state,
  };
} catch (error) {
  return {
    notify: `QWC-Watchdog-Alarm: Die begrenzte Reparatur für ${singleHost}/${role} konnte nicht abgeschlossen werden. Weitere Nodes wurden nicht angefasst. Fehler: ${String(error)}`,
    state,
  };
}
