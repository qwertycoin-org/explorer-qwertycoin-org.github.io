# Qwertycoin Explorer Operations

This explorer is a read-only observer. It must never change consensus settings,
wallet data, service identities, or blockchain storage.

## Release pair and build

1. Record the reviewed 40-character explorer SHA and the compatible Qwertycoin
   core SHA. The current compatibility candidate is
   `e6e0b46b6603bc5c1402df63696b514ba735f8ee`, the merge commit for the final
   v2 genesis and EPoSE profile.
2. Resolve the deployment architecture and the matching immutable Ubuntu image
   digest. Copy `BUILD.env.example` outside the repository, fill the immutable
   values, and do not put credentials in it.
3. Build with explicit arguments and retain the build log and exit status. The
   Dockerfile rejects missing source SHAs and runs the regression test target.
   Build only from a clean detached checkout of that SHA:
   ```sh
   test -z "$(git status --porcelain)"
   test "$(git rev-parse HEAD)" = "EXPLORER_SHA_RECORDED_IN_RELEASE"
   docker build --pull --file Dockerfile.node01 \
     --build-arg UBUNTU_IMAGE="UBUNTU_IMAGE_WITH_SHA256_DIGEST" \
     --build-arg QWC_COMMIT="CORE_SHA_RECORDED_IN_RELEASE" \
     --build-arg EXPLORER_SOURCE_SHA="EXPLORER_SHA_RECORDED_IN_RELEASE" \
     --tag qwertycoin-explorer:candidate .
   ```
4. Record the resulting image ID, repository digest when a registry push is
   used, architecture, OCI source labels, dependency versions, and
   `/api/v1/version` output. A local-only preview may use the immutable
   `sha256:<64-hex-image-id>` directly. Do not claim
   byte-for-byte reproducibility while apt package snapshots remain unpinned.

## Read-only preflight

Before any host mutation, verify the host key against the trusted inventory.
Never disable host-key checking or replace a mismatched key without independent
verification. Record, with environment values redacted:

- current container ID, immutable image digest, effective UID/GID, restart and
  health state, networks, loopback port, resource limits, and log policy;
- exact chain mount source and destination, confirming `RW=false` and that it is
  the existing chain rather than a newly created directory or volume;
- actual Compose/env files and the complete nginx/TLS include chain;
- daemon listener topology and that the explorer uses a restricted observer RPC;
- writable explorer-derived storage, its effective UID/GID, free space and an
  operator-controlled chain reset ID distinct from previous rehearsals;
- current public DNS, certificate identity/expiry, headers, routes, and reported
  explorer/core builds.

Never include full container environments or secret-bearing command lines in a
report.

## Parallel preview

Use `docker-compose.preview.yml` with the exact project name
`qwertycoin-explorer-preview` and `/etc/qwertycoin-explorer/preview.env`:

- immutable candidate repository digest or local image ID;
- a container name not already present on the host (the September follow-up uses
  `qwertycoin-explorer-followup`);
- a free loopback host port (the September follow-up uses `29984` because
  `29983` is already occupied);
- the verified current chain path, read-only;
- a distinct writable explorer-derived volume for supply checkpoints;
- the current operator chain reset ID;
- the verified restricted daemon URL;
- the deployment UID/GID established during preflight.

Validate all immutable/external inputs before `up`; these commands must succeed:

```sh
grep -Eq '^QWC_EXPLORER_IMAGE=([^[:space:]]+@)?sha256:[0-9a-f]{64}$' /etc/qwertycoin-explorer/preview.env
docker volume inspect VERIFIED_CHAIN_VOLUME_NAME >/dev/null
! docker volume inspect NEW_DERIVED_VOLUME_NAME >/dev/null 2>&1
docker volume create --label org.qwertycoin.role=explorer-derived NEW_DERIVED_VOLUME_NAME >/dev/null
docker volume inspect VERIFIED_DERIVED_VOLUME_NAME >/dev/null
docker network inspect VERIFIED_DAEMON_NETWORK_NAME >/dev/null
docker compose --project-name qwertycoin-explorer-preview \
  --env-file /etc/qwertycoin-explorer/preview.env \
  --file /opt/qwertycoin-explorer/docker-compose.preview.yml config --quiet
docker compose --project-name qwertycoin-explorer-preview \
  --env-file /etc/qwertycoin-explorer/preview.env \
  --file /opt/qwertycoin-explorer/docker-compose.preview.yml up --detach --no-build
docker inspect DISTINCT_PREVIEW_CONTAINER_NAME \
  --format '{{.Image}} {{range .Mounts}}{{.Destination}} rw={{.RW}} {{end}}'
```

Check `docker compose version` before relying on those commands. If the
verified host has neither the Compose plugin nor `docker-compose`, do not add a
new package during cutover. Start the preview with an explicit `docker run`
invocation carrying the same immutable image ID, name, loopback port, read-only
chain mount, daemon network, UID/GID, read-only root, tmpfs, dropped
capabilities, `no-new-privileges`, PID/CPU/RAM limits, restart policy, stop
timeout, health check from the image, and bounded log rotation. Record the
exact invocation in the release evidence. Validate the resulting container
with the same `docker inspect` command above. Stop it with
`docker stop --time 30 DISTINCT_PREVIEW_CONTAINER_NAME` during rollback.

The final inspection must show the recorded candidate image ID, the verified
existing chain mount with `rw=false`, and only the explorer-derived mount with
`rw=true`. A different Compose project name and container name are mandatory.

Do not use `docker compose down -v`, create a replacement chain volume, loosen
chain permissions, or restart a core daemon. Keep the candidate private on
loopback or an authenticated operator tunnel.

Verify `/healthz`, `/readyz`, `/api/v1/version`, chain identity, overview, block,
transaction, POST search, mempool, service nodes, epochs, both themes, keyboard
navigation, 360/390/768/1440 layouts, retired secret routes, and absence of an
unrestricted RPC proxy. The `/qwc-rpc/` compatibility adapter must forward only
the explicit wallet path and parsed JSON-RPC method allowlists to the verified
restricted daemon listener. Unknown paths and methods, batch requests,
notifications, malformed envelopes and unsupported HTTP methods must be rejected
before daemon work. Verify the deployed web wallet can call `get_info`, fetch
binary sync data, and receive a daemon-level rejection for a deliberately
malformed transaction without logging any request body.
Observe two refresh intervals and a real block when available.
Record CPU/RAM, response latency, upstream RPC rate, and daemon impact under a
declared traffic ceiling.

## Public switch

Install the repository's separate explorer-frontend and wallet-gateway upstream
snippets. Keeping them distinct allows a frontend rollback without bypassing the
parsed wallet policy. Before the switch, preserve both known-good snippets with:

```sh
sudo cp --preserve=mode,ownership,timestamps \
  /etc/nginx/snippets/qwertycoin-explorer-upstream.conf \
  /etc/nginx/snippets/qwertycoin-explorer-upstream.conf.pre-candidate
sudo cp --preserve=mode,ownership,timestamps \
  /etc/nginx/snippets/qwertycoin-wallet-rpc-upstream.conf \
  /etc/nginx/snippets/qwertycoin-wallet-rpc-upstream.conf.pre-candidate
```

Change both snippets' sole servers to the private candidate port recorded in the
release evidence (`127.0.0.1:29984` for the September follow-up), then run:

```sh
sudo nginx -t
sudo systemctl reload nginx
```

If the live vhost did not previously use the shared snippet, back up the full
enabled vhost before adding the include. Record that backup path. Its rollback
is to restore the full vhost, run `nginx -t`, and reload only after the test
succeeds; the previous vhost remains active until reload.

Do not reload unless `nginx -t` succeeds. Do not restart nginx or the daemon.
Verify the public HTTPS site in a fresh browser session and confirm that the
footer and API report the tested source pair.

Keep the prior compatible container, digest, configuration, and upstream value
for the observation window.

## Rollback

Rollback triggers include wrong chain identity, incorrect monetary attribution,
reachable secret/admin routes, repeated 5xx responses, broken search/block/tx
journeys, or material daemon impact.

1. Restore and validate only the previous explorer **frontend** upstream:
   ```sh
   sudo cp /etc/nginx/snippets/qwertycoin-explorer-upstream.conf.pre-candidate \
     /etc/nginx/snippets/qwertycoin-explorer-upstream.conf
   sudo nginx -t
   sudo systemctl reload nginx
   ```
2. Verify that `/readyz` and the public site now resolve through the restored
   backend.
3. Verify public HTTPS recovery in a fresh session.
4. Stop only the failed candidate after recovery is confirmed:
   ```sh
   docker compose --project-name qwertycoin-explorer-preview \
     --env-file /etc/qwertycoin-explorer/preview.env \
     --file /opt/qwertycoin-explorer/docker-compose.preview.yml stop
   ```
   On a host without Compose, use
   `docker stop --time 30 DISTINCT_PREVIEW_CONTAINER_NAME` instead.
5. Invalidate only explorer frontend or derived caches.

Keep the parsed wallet gateway on the tested candidate during a frontend
rollback. Restore its separate upstream only to another tested parser gateway,
never to the restricted daemon directly. Never restore a historical full-vhost
backup that reopens the generic daemon proxy.

Preserve the derived-index volume during rollback. If the index calculation
version or chain-reset context changes, stop only the explorer, retain the old
index for evidence, create a distinct empty explorer-owned volume, rebuild to a
verified canonical anchor and only then restore readiness. Never delete or write
the blockchain volume.

Never roll back or reset the blockchain, daemon, wallets, or service identities
to repair the explorer. If the previous artifact is incompatible with the core,
serve an honest maintenance page until a compatible observer is ready.

## Planned chain reset

The final chain reset is a separately authorized core operation. During it, stop
the explorer reader, retain its configuration, and reopen it only after the chain
is ready. Rebuild only explorer-derived indexes/caches, then revalidate genesis,
network and parameter identity, tip context, EPoSE capability, links, and the
mainnet-v2 launch notice. A same-genesis reset also requires a new local deployment
generation so derived data cannot leak across deployment generations.
