# Qwertycoin Explorer

This repository contains the Qwertycoin v2 explorer used for observing the
public chain, mempool, RandomX blocks, and EPoSE service-node state.

The explorer is an observer only. It does not participate in consensus, does not
qualify service nodes, and must not be treated as an EPoSE authority.

## Privacy and Security

The public explorer does not use analytics or third-party tracking scripts. Some
inherited diagnostic features can process a private view key or transaction
private key on the server. Use those features only if you trust the explorer
operator; prefer wallet-side verification when available. Sensitive keys are
accepted only in POST bodies, are redacted from rendered responses and error
logs, and must never be sent in URLs.

The sensitive JSON helpers `/api/outputs` and `/api/outputsblocks` therefore
accept URL-encoded `POST` bodies only. They intentionally do not support their
inherited `GET` query-string form.

The deployment profiles bind the explorer to loopback on the host. Keep the
daemon's administrative RPC private; expose only the explorer and a separately
configured restricted RPC endpoint. The explorer observes node state and is
never a consensus source of truth.

## Current Scope

- Qwertycoin block and transaction views
- JSON API for block, transaction, mempool, network, fee, emission, and EPoSE
  observer data
- EPoSE dashboard for registry, attestation, qualification, snapshot, and reward
  visibility
- Optional autorefresh for monitoring deployments
- Optional emission monitor based on local LMDB access

## node01 Docker Run

The node01 deployment uses `Dockerfile.node01` and
`docker-compose.node01.yml`. It mounts the existing Qwertycoin node chain volume
and connects to the Qwertycoin mainnet-mode daemon RPC.

```bash
docker compose -f docker-compose.node01.yml up -d --build
```

Default node01 explorer settings:

```text
HTTP:        127.0.0.1:29982 -> 8081
Chain path:  /home/qwertycoin/.qwertycoin/lmdb
Daemon RPC:  http://qwertycoin-mainnet:8197
```

The node01 mainnet-mode daemon currently writes its Docker volume as root. The
explorer compose file therefore runs the explorer container as root for this
deployment profile so LMDB lock handling can open the mounted chain database.
The chain volume is mounted read-only so the explorer cannot modify canonical
node state.

## explorer.qwertycoin.org Docker Run

The production profile for `explorer.qwertycoin.org` is prepared in
`docker-compose.production.yml`. It is meant for a host where the Qwertycoin
daemon already runs on the server and nginx terminates public HTTP/TLS traffic.

```bash
cp deploy/explorer.qwertycoin.org.env.example .env
docker compose -f docker-compose.production.yml --env-file .env up -d --build
```

Default production settings:

```text
HTTP:        127.0.0.1:29982 -> 8081
Chain path:  /var/lib/qwertycoin/.qwertycoin/lmdb
Daemon RPC:  http://host.docker.internal:8197
```

If the daemon stores the chain elsewhere, set `QWC_CHAIN_PATH` in `.env` to the
directory containing the `lmdb` folder. If the daemon is also running in Docker,
prefer connecting both containers to the same Docker network and set
`QWC_DAEMON_URL` to the daemon container name, as done in the node01 profile.
The production profile runs as the image's unprivileged UID/GID 101 by default;
grant that account read access to the chain directory instead of running the
container as root.

An nginx vhost template is available at
`deploy/explorer.qwertycoin.org.nginx.conf`. After the DNS A record points to the
server, enable TLS with certbot or the server's existing certificate workflow.

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
  ..
make -j2
```

Run against a local Qwertycoin mainnet-mode daemon:

```bash
./qwertycoin-explorer \
  --port 8081 \
  --bc-path /home/qwertycoin/.qwertycoin/lmdb \
  --daemon-url http://127.0.0.1:8197 \
  --enable-json-api \
  --enable-autorefresh-option
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
