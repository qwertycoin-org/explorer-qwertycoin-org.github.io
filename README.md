# Qwertycoin Explorer

This repository contains the Qwertycoin v2 explorer used for observing the
public chain, mempool, RandomX blocks, and EPoSE service-node state.

The explorer is an observer only. It does not participate in consensus, does not
qualify service nodes, and must not be treated as an EPoSE authority.

## Privacy and Security

The public explorer does not use analytics or third-party tracking scripts. It
does not accept wallet private view keys, transaction private keys, operator
keys, service keys, seeds, or other secrets in URLs, request bodies, or browser
messages. Output decoding and transaction-proof creation belong in a trusted
wallet and are deliberately unavailable here.

Inherited secret-processing routes, including `/myoutputs`, `/prove`,
`/api/outputs`, `/api/outputsblocks`, and the server-side key-file checkers, are
not registered by the public explorer binary. Reverse proxies must not retain
compatibility routes for them and should avoid logging query strings.

The deployment profiles bind the explorer to loopback on the host. Keep the
daemon's administrative RPC private; expose only the explorer and a separately
configured restricted RPC endpoint. The explorer observes node state and is
never a consensus source of truth.

## Current Scope

- Qwertycoin block and transaction views
- Versioned, allowlisted JSON reads for bounded block/mempool, network, build,
  and EPoSE observer data
- Stable-identity EPoSE views with explicit unsupported/unavailable states
- Exact eight-decimal QWC formatting and one-row-per-block navigation
- Persistent light/dark theme and responsive, keyboard-accessible layouts

See [the metric dictionary](docs/METRICS.md), [API v1](docs/API_V1.md), and
[bounded core dependencies](docs/CORE_API_DEPENDENCIES.md).

## node01 Docker Run

The node01 deployment uses `Dockerfile.node01` and
`docker-compose.node01.yml`. It mounts the existing Qwertycoin node chain volume
and connects to the Qwertycoin mainnet-mode daemon RPC.

The build requires explicit full explorer/core SHAs and a digest-pinned base
image; see `deploy/BUILD.env.example`. Do not use this profile as the production
switch procedure.

Default node01 explorer settings:

```text
HTTP:        127.0.0.1:29982 -> 8081
Chain path:  /home/qwertycoin/.qwertycoin/lmdb
Daemon RPC:  verified restricted observer endpoint
```

The chain volume remains read-only. Supply the verified deployment UID/GID and
grant only the read access needed for LMDB; do not run the explorer as root to
work around an access failure.

## explorer.qwertycoin.org Docker Run

The production profile for `explorer.qwertycoin.org` is prepared in
`docker-compose.production.yml`. It is meant for a host where the Qwertycoin
daemon already runs on the server and nginx terminates public HTTP/TLS traffic.

Production consumes one already-tested immutable image digest. Copy
`deploy/explorer.qwertycoin.org.env.example` outside the repository, fill every
required value from the live inventory, and follow `deploy/RUNBOOK.md` for the
parallel preview, public switch, and rollback. The live host's existing named
chain volume is consumed only through `docker-compose.preview.yml`, where it is
declared `external: true` so a typo cannot silently create a replacement chain.

Default production settings:

```text
HTTP:        127.0.0.1:29982 -> 8081
Chain path:  /var/lib/qwertycoin/.qwertycoin/lmdb
Daemon RPC:  verified restricted observer endpoint (currently port 8198 on the reviewed host)
```

`QWC_CHAIN_PATH`, daemon URL, UID/GID, loopback port, container name, and image
digest have no operational defaults. Resolve them from the actual host. Never
replace the existing chain mount with an empty directory or new volume.

The nginx file is a mergeable reference, not a replacement for the real TLS
vhost. Preserve the existing certificate workflow and explicitly retire legacy
secret routes. The `/qwc-rpc/` compatibility adapter is intentionally limited to
the web wallet's path allowlist and the verified restricted daemon listener; it
must never point at the administrative RPC port or become a catch-all proxy.

## Local Build

The code still links against Qwertycoin's Monero-derived core libraries. The
CMake variable names retain their upstream `MONERO_*` names for compatibility
with the inherited build scripts.

```bash
mkdir -p build
cd build
cmake \
  -DMONERO_DIR=/path/to/qwertycoin \
  -DMONERO_SOURCE_DIR=/path/to/qwertycoin \
  -DMONERO_BUILD_DIR=/path/to/qwertycoin/build/x86_64-linux-gnu/release \
  -DQWC_SOURCE_SHA=full-compatible-core-sha \
  ..
make -j2
ctest --output-on-failure
```

Run against a local Qwertycoin mainnet-mode daemon:

```bash
./qwertycoin-explorer \
  --port 8081 \
  --bc-path /home/qwertycoin/.qwertycoin/lmdb \
  --daemon-url http://127.0.0.1:8198 \
  --enable-json-api
```

## Branding Notes

Public UI, help text, Docker examples, and Qwertycoin-facing documentation use
Qwertycoin/QWC terminology.

Some inherited internal identifiers intentionally still use upstream names such
as `xmreg`, `MONERO_*`, or Monero file magic strings. These are kept where they
are build internals, source compatibility names, or inherited CryptoNote/Monero
file-format identifiers rather than public Qwertycoin product branding. The
shipped executable is named `qwertycoin-explorer`.

## Upstream Attribution

This explorer is derived from the Onion Monero Blockchain Explorer and keeps the
upstream architecture, inherited internal names, and applicable attribution.
Existing upstream copyright and license obligations remain intact.
