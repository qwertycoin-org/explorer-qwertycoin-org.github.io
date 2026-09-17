#!/usr/bin/env bash
set -euo pipefail

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
umask 077

readonly role_label='org.qwertycoin.role=public-explorer'
readonly chain_destination='/home/qwertycoin/.qwertycoin'
readonly derived_destination='/var/lib/qwertycoin-explorer'
readonly state_directory="${XDG_STATE_HOME:-${HOME}/.local/state}/qwc-explorer-deploy"
readonly active_file="${state_directory}/active-container"

fail() {
  printf 'DEPLOY_ERROR\n' >&2
  exit 1
}

valid_sha() {
  [[ $1 =~ ^[0-9a-f]{40}$ ]]
}

load_image() {
  docker load --quiet >/dev/null 2>&1 || fail
  printf 'IMAGE_OK\n'
}

rollout() {
  local explorer_sha=$1
  valid_sha "${explorer_sha}" || fail
  [[ -f "${active_file}" ]] || fail

  local current
  IFS= read -r current <"${active_file}"
  [[ ${current} =~ ^qwertycoin-explorer-[a-z0-9-]+$ ]] || fail

  local image_tag="qwertycoin-explorer:deploy-${explorer_sha}"
  local image_id explorer_revision core_revision
  image_id=$(docker image inspect "${image_tag}" --format '{{.Id}}') || fail
  explorer_revision=$(docker image inspect "${image_id}" \
    --format '{{index .Config.Labels "org.opencontainers.image.revision"}}') || fail
  core_revision=$(docker image inspect "${image_id}" \
    --format '{{index .Config.Labels "org.qwertycoin.core.revision"}}') || fail
  [[ ${explorer_revision} == "${explorer_sha}" ]] || fail
  valid_sha "${core_revision}" || fail
  [[ $(docker image inspect "${image_id}" --format '{{.Architecture}}/{{.Os}}') == 'amd64/linux' ]] || fail

  docker inspect "${current}" >/dev/null 2>&1 || fail
  [[ $(docker inspect "${current}" --format '{{.State.Running}}') == true ]] || fail
  [[ $(docker inspect "${current}" --format '{{.State.Health.Status}}') == healthy ]] || fail
  [[ $(docker inspect "${current}" --format '{{.HostConfig.ReadonlyRootfs}}') == true ]] || fail
  [[ $(docker inspect "${current}" --format '{{.Config.User}}') == '101:101' ]] || fail
  [[ $(docker inspect "${current}" --format '{{.HostConfig.RestartPolicy.Name}}') == 'unless-stopped' ]] || fail
  [[ $(docker inspect "${current}" | jq -r \
    '[.[0].HostConfig.CapDrop[]? | select(. == "ALL")] | length') == 1 ]] || fail
  [[ $(docker inspect "${current}" | jq -r \
    '[.[0].HostConfig.SecurityOpt[]? | select(. == "no-new-privileges:true")] | length') == 1 ]] || fail

  local current_image current_revision current_core_revision
  current_image=$(docker inspect "${current}" --format '{{.Image}}') || fail
  current_revision=$(docker image inspect "${current_image}" \
    --format '{{index .Config.Labels "org.opencontainers.image.revision"}}') || fail
  current_core_revision=$(docker image inspect "${current_image}" \
    --format '{{index .Config.Labels "org.qwertycoin.core.revision"}}') || fail
  valid_sha "${current_revision}" || fail
  [[ ${current_core_revision} == "${core_revision}" ]] || fail

  if [[ ${current_revision} == "${explorer_sha}" ]]; then
    printf 'ALREADY_CURRENT\n'
    return
  fi

  local network_name explorer_ip chain_volume derived_volume host_ip host_port
  IFS=$'\t' read -r network_name explorer_ip < <(docker inspect "${current}" | jq -er '
    .[0].NetworkSettings.Networks | to_entries
    | select(length == 1) | .[0] | [.key, .value.IPAddress] | @tsv') || fail
  [[ -n ${network_name} && -n ${explorer_ip} ]] || fail
  chain_volume=$(docker inspect "${current}" | jq -er --arg dst "${chain_destination}" '
    [.[0].Mounts[] | select(.Destination == $dst and .RW == false and .Type == "volume")]
    | select(length == 1) | .[0].Name') || fail
  derived_volume=$(docker inspect "${current}" | jq -er --arg dst "${derived_destination}" '
    [.[0].Mounts[] | select(.Destination == $dst and .RW == true and .Type == "volume")]
    | select(length == 1) | .[0].Name') || fail
  IFS=$'\t' read -r host_ip host_port < <(docker inspect "${current}" | jq -er '
    .[0].HostConfig.PortBindings["8081/tcp"]
    | select(length == 1) | .[0] | [.HostIp, .HostPort] | @tsv') || fail
  [[ ${host_ip} == '127.0.0.1' && ${host_port} =~ ^[0-9]{2,5}$ ]] || fail

  local -a explorer_cmd
  mapfile -t explorer_cmd < <(docker inspect "${current}" | jq -er '.[0].Config.Cmd[]') || fail
  ((${#explorer_cmd[@]} > 0)) || fail

  local new_name="qwertycoin-explorer-${explorer_sha:0:12}"
  local stamp rollback_name failed_name
  stamp=$(date -u +%Y%m%dT%H%M%SZ)
  rollback_name="${current}-rollback-${stamp}"
  failed_name="${new_name}-failed-${stamp}"
  ! docker inspect "${new_name}" >/dev/null 2>&1 || fail
  ! docker inspect "${rollback_name}" >/dev/null 2>&1 || fail

  local mutated=0 old_disconnected=0
  rollback() {
    local rc=$?
    trap - ERR
    set +e
    if ((mutated == 1)); then
      if docker inspect "${new_name}" >/dev/null 2>&1; then
        docker stop --time 30 "${new_name}" >/dev/null 2>&1
        docker rename "${new_name}" "${failed_name}" >/dev/null 2>&1
      fi
      if docker inspect "${rollback_name}" >/dev/null 2>&1; then
        if ((old_disconnected == 1)); then
          docker network connect --ip "${explorer_ip}" \
            "${network_name}" "${rollback_name}" >/dev/null 2>&1
        fi
        docker rename "${rollback_name}" "${current}" >/dev/null 2>&1
        docker start "${current}" >/dev/null 2>&1
      fi
    fi
    printf 'DEPLOY_ERROR\n' >&2
    exit "${rc}"
  }
  trap rollback ERR

  docker stop --time 30 "${current}" >/dev/null
  docker rename "${current}" "${rollback_name}" >/dev/null
  mutated=1
  docker network disconnect "${network_name}" "${rollback_name}" >/dev/null
  old_disconnected=1

  docker run --detach \
    --name "${new_name}" \
    --label "${role_label}" \
    --label "org.qwertycoin.source.revision=${explorer_sha}" \
    --restart unless-stopped \
    --stop-timeout 30 \
    --read-only \
    --tmpfs /tmp:rw,nosuid,nodev,noexec,size=32m,mode=1777 \
    --cap-drop ALL \
    --security-opt no-new-privileges:true \
    --pids-limit 256 \
    --memory 2g \
    --cpus 2 \
    --log-driver json-file \
    --log-opt max-size=10m \
    --log-opt max-file=3 \
    --health-cmd 'curl --fail --silent --show-error http://127.0.0.1:8081/readyz' \
    --health-interval 30s \
    --health-timeout 5s \
    --health-retries 3 \
    --health-start-period 30s \
    --user 101:101 \
    --network "${network_name}" \
    --ip "${explorer_ip}" \
    --publish "${host_ip}:${host_port}:8081" \
    --volume "${chain_volume}:${chain_destination}:ro" \
    --volume "${derived_volume}:${derived_destination}" \
    "${image_id}" "${explorer_cmd[@]}" >/dev/null

  local ready=0 ready_body
  for _ in $(seq 1 300); do
    if ready_body=$(curl --fail --silent --show-error --max-time 5 \
        "http://${host_ip}:${host_port}/readyz" 2>/dev/null) \
      && jq -e '.status == "success"
        and .data.identity.compatible == true
        and .data.identity.genesis_matches == true
        and .data.wallet_rpc.status == "ready"
        and .data.supply.complete == true' >/dev/null <<<"${ready_body}"; then
      ready=1
      break
    fi
    sleep 1
  done
  [[ ${ready} == 1 ]] || false

  for _ in $(seq 1 300); do
    [[ $(docker inspect "${new_name}" --format '{{.State.Health.Status}}') == healthy ]] && break
    sleep 1
  done
  [[ $(docker inspect "${new_name}" --format '{{.State.Health.Status}}') == healthy ]]
  [[ $(docker inspect "${new_name}" --format '{{.Image}}') == "${image_id}" ]]
  [[ $(docker inspect "${new_name}" --format '{{.RestartCount}}') == 0 ]]

  local version info nodes rewards snapshot_ok=0
  version=$(curl --fail --silent --show-error --max-time 10 \
    "http://${host_ip}:${host_port}/api/v1/version")
  jq -e --arg explorer "${explorer_sha}" --arg core "${core_revision}" '
    .status == "success"
    and .data.explorer_source_sha == $explorer
    and .data.qwc_source_sha == $core' >/dev/null <<<"${version}"

  for _ in $(seq 1 10); do
    info=$(curl --fail --silent --show-error --max-time 20 \
      "http://${host_ip}:${host_port}/api/v1/epose")
    nodes=$(curl --fail --silent --show-error --max-time 20 \
      "http://${host_ip}:${host_port}/api/v1/epose/service-nodes")
    rewards=$(curl --fail --silent --show-error --max-time 20 \
      "http://${host_ip}:${host_port}/api/v1/epose/rewards")
    if jq -e -n --argjson info "${info}" --argjson nodes "${nodes}" --argjson rewards "${rewards}" '
      ($info.status == "success")
      and ($nodes.status == "success")
      and ($rewards.status == "success")
      and ($info.data.snapshot_consistency == "anchored")
      and ($nodes.data.snapshot_consistency == "anchored")
      and ($rewards.data.snapshot_consistency == "anchored")
      and ($info.data.snapshot_block_count == $nodes.data.snapshot_block_count)
      and ($info.data.snapshot_block_count == $rewards.data.snapshot_block_count)
      and ($info.data.snapshot_tip_height == $nodes.data.snapshot_tip_height)
      and ($info.data.snapshot_tip_height == $rewards.data.snapshot_tip_height)
      and ($info.data.snapshot_tip_hash == $nodes.data.snapshot_tip_hash)
      and ($info.data.snapshot_tip_hash == $rewards.data.snapshot_tip_hash)
      and ($rewards.data.height == $info.data.snapshot_block_count)
      and ($nodes.data.current_epoch == $info.data.current_epoch)
      and ($nodes.data.source_epoch == $rewards.data.epoch)
      and ($rewards.data.epoch + 1 == $info.data.current_epoch)' >/dev/null; then
      snapshot_ok=1
      break
    fi
    sleep 1
  done
  [[ ${snapshot_ok} == 1 ]] || false

  [[ $(docker inspect "${rollback_name}" --format '{{.State.Running}}') == false ]]
  local next_active="${active_file}.next.$$"
  printf '%s\n' "${new_name}" >"${next_active}"
  mv "${next_active}" "${active_file}"

  trap - ERR
  printf 'ROLLOUT_OK\n'
}

main() {
  mkdir -p "${state_directory}"
  case ${SSH_ORIGINAL_COMMAND:-} in
    image-load)
      load_image
      ;;
    rollout\ *)
      local command_name explorer_sha extra
      read -r command_name explorer_sha extra <<<"${SSH_ORIGINAL_COMMAND}"
      [[ ${command_name} == rollout && -z ${extra:-} ]] || fail
      rollout "${explorer_sha}"
      ;;
    *)
      fail
      ;;
  esac
}

main "$@"
