# Qwertycoin node watchdog

This gateway-side watchdog complements Docker's `unless-stopped` restart policy.
Docker handles exited containers and daemon/host restarts. The watchdog detects
containers that are still running but no longer serve a valid QWC Core, Explorer,
or Wallet Gateway response.

## Safety contract

- exact production container names and image IDs only;
- expected QWC mainnet Genesis is validated before any repair;
- missing containers, unexpected images, restart-policy drift, Genesis mismatch,
  a peerless Core, and application-readiness failures with a live process are
  alert-only;
- automatic Explorer/Gateway repair is limited to a stopped container or failed
  process liveness; indexing lag and upstream readiness never trigger a restart;
- service failures must be observed by three consecutive one-minute checks;
- at most one component on one seed is repaired per scheduler run;
- a Core is restarted only when at least two other Cores are healthy and use the
  expected Genesis;
- Explorer/Gateway repair requires the local Core to be healthy;
- every action is followed by a functional recheck;
- rollback containers and unrelated shared-host workloads are never selected.

## Maintenance pause

Create `/workspace/state/qwc-node-watchdog/PAUSED` before intentionally stopping
production containers. Remove or rename it after maintenance. Healthy scheduler
runs are silent; interventions and unrecoverable conditions are delivered to the
configured operator route.

## Manual inspection

```sh
/workspace/ops/qwc-node-watchdog/watchdog.sh probe | jq .
```

The `repair` subcommand is intended for the automation and always revalidates all
invariants and quorum immediately before taking action.

`automation.js` is the scheduler payload. It keeps failure counters in scheduler
state, requires three consecutive service failures, applies per-target cooldowns,
and returns no notification for healthy checks.
