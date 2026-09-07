# Qwertycoin Explorer Operations

This explorer is a read-only observer. It must never change consensus settings,
wallet data, service identities, or blockchain storage.

## Release pair and build

1. Record the reviewed 40-character explorer SHA and the compatible Qwertycoin
   core SHA. The current compatibility candidate is
   `ef5eb745ca9fec01752cd3e555cfa2b60741efac`; revalidate it after the core work
   reaches a stable recorded commit.
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
- current public DNS, certificate identity/expiry, headers, routes, and reported
  explorer/core builds.

Never include full container environments or secret-bearing command lines in a
report.

## Parallel preview

Use `docker-compose.preview.yml` with the exact project name
`qwertycoin-explorer-preview` and `/etc/qwertycoin-explorer/preview.env`:

- immutable candidate repository digest or local image ID;
- container name `qwertycoin-explorer-preview`;
- loopback host port `29983`;
- the verified current chain path, read-only;
- the verified restricted daemon URL;
- the deployment UID/GID established during preflight.

Validate all immutable/external inputs before `up`; these commands must succeed:

```sh
grep -Eq '^QWC_EXPLORER_IMAGE=([^[:space:]]+@)?sha256:[0-9a-f]{64}$' /etc/qwertycoin-explorer/preview.env
docker volume inspect VERIFIED_CHAIN_VOLUME_NAME >/dev/null
docker network inspect VERIFIED_DAEMON_NETWORK_NAME >/dev/null
docker compose --project-name qwertycoin-explorer-preview \
  --env-file /etc/qwertycoin-explorer/preview.env \
  --file /opt/qwertycoin-explorer/docker-compose.preview.yml config --quiet
docker compose --project-name qwertycoin-explorer-preview \
  --env-file /etc/qwertycoin-explorer/preview.env \
  --file /opt/qwertycoin-explorer/docker-compose.preview.yml up --detach --no-build
docker inspect qwertycoin-explorer-preview \
  --format '{{.Image}} {{range .Mounts}}{{if eq .Destination "/home/qwertycoin/.qwertycoin"}}{{.Name}} rw={{.RW}}{{end}}{{end}}'
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
`docker stop --time 30 qwertycoin-explorer-preview` during rollback.

The final inspection must show the recorded candidate image ID, the verified
existing volume and `rw=false`. A different Compose project name is mandatory;
changing only `container_name` is insufficient.

Do not use `docker compose down -v`, create a replacement chain volume, loosen
chain permissions, or restart a core daemon. Keep the candidate private on
loopback or an authenticated operator tunnel.

Verify `/healthz`, `/readyz`, `/api/v1/version`, chain identity, overview, block,
transaction, POST search, mempool, service nodes, epochs, both themes, keyboard
navigation, 360/390/768/1440 layouts, retired secret routes, and absence of a
generic RPC proxy. Observe two refresh intervals and a real block when available.
Record CPU/RAM, response latency, upstream RPC rate, and daemon impact under a
declared traffic ceiling.

## Public switch

Install the repository's shared upstream snippet so every proxied route uses the
same backend. Before the switch, preserve the known-good snippet with:

```sh
sudo cp --preserve=mode,ownership,timestamps \
  /etc/nginx/snippets/qwertycoin-explorer-upstream.conf \
  /etc/nginx/snippets/qwertycoin-explorer-upstream.conf.pre-candidate
```

Change the snippet's sole server from `127.0.0.1:29982` to
`127.0.0.1:29983`, then run exactly:

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

1. Restore and validate the previous explorer upstream with these exact commands:
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
   `docker stop --time 30 qwertycoin-explorer-preview` instead.
5. Invalidate only explorer frontend or derived caches.

Never roll back or reset the blockchain, daemon, wallets, or service identities
to repair the explorer. If the previous artifact is incompatible with the core,
serve an honest maintenance page until a compatible observer is ready.

## Planned chain reset

The final chain reset is a separately authorized core operation. During it, stop
the explorer reader, retain its configuration, and reopen it only after the chain
is ready. Rebuild only explorer-derived indexes/caches, then revalidate genesis,
network and parameter identity, tip context, EPoSE capability, links, and the
rehearsal/launch notice. A same-genesis reset also requires a new local deployment
generation so derived data cannot leak across rehearsals.
