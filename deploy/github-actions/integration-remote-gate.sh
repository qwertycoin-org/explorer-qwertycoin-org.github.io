#!/usr/bin/env bash
set -euo pipefail

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
umask 077

readonly expected_core_sha='82cf8703c895663cbe69347188448b5f00f7a0e8'
readonly role_value='integration-explorer'
readonly role_label="org.qwertycoin.role=${role_value}"
readonly production_role_value='public-explorer'
readonly chain_destination='/home/qwertycoin/.qwertycoin'
readonly derived_destination='/var/lib/qwertycoin-explorer'
readonly configuration_file="${XDG_CONFIG_HOME:-${HOME}/.config}/qwc-explorer-integration/deploy.env"
readonly state_directory="${XDG_STATE_HOME:-${HOME}/.local/state}/qwc-explorer-integration-deploy"
readonly active_file="${state_directory}/active-container"

fail() {
  printf 'DEPLOY_ERROR\n' >&2
  exit 1
}

valid_sha() {
  [[ $1 =~ ^[0-9a-f]{40}$ ]]
}

load_configuration() {
  [[ -f ${configuration_file} && ! -L ${configuration_file} ]] || fail
  [[ $(stat -c '%a' "${configuration_file}") == '600' ]] || fail
  # shellcheck disable=SC1090
  source "${configuration_file}"
  [[ ${PRODUCTION_ACTIVE_FILE:-} == /* ]] || fail
  [[ ${INTEGRATION_DERIVED_VOLUME:-} =~ ^[a-zA-Z0-9][a-zA-Z0-9_.-]+$ ]] || fail
  [[ ${INTEGRATION_HOST_IP:-} == '127.0.0.1' ]] || fail
  [[ ${INTEGRATION_HOST_PORT:-} =~ ^[0-9]{2,5}$ ]] || fail
}

load_image() {
  docker load --quiet >/dev/null 2>&1 || fail
  printf 'IMAGE_OK\n'
}

rollout() {
  local explorer_sha=$1
  valid_sha "${explorer_sha}" || fail
  load_configuration

  local image_tag="qwertycoin-explorer:integration-deploy-${explorer_sha}"
  local image_id explorer_revision core_revision
  image_id=$(docker image inspect "${image_tag}" --format '{{.Id}}') || fail
  explorer_revision=$(docker image inspect "${image_id}" \
    --format '{{index .Config.Labels "org.opencontainers.image.revision"}}') || fail
  core_revision=$(docker image inspect "${image_id}" \
    --format '{{index .Config.Labels "org.qwertycoin.core.revision"}}') || fail
  [[ ${explorer_revision} == "${explorer_sha}" ]] || fail
  [[ ${core_revision} == "${expected_core_sha}" ]] || fail
  [[ $(docker image inspect "${image_id}" --format '{{.Architecture}}/{{.Os}}') == 'amd64/linux' ]] || fail

  [[ -f ${PRODUCTION_ACTIVE_FILE} ]] || fail
  local production
  IFS= read -r production <"${PRODUCTION_ACTIVE_FILE}"
  [[ ${production} =~ ^qwertycoin-explorer-[a-z0-9-]+$ ]] || fail
  docker inspect "${production}" >/dev/null 2>&1 || fail
  [[ $(docker inspect "${production}" --format '{{.State.Running}}') == true ]] || fail
  [[ $(docker inspect "${production}" --format '{{.State.Health.Status}}') == healthy ]] || fail
  [[ $(docker inspect "${production}" --format '{{index .Config.Labels "org.qwertycoin.role"}}') == "${production_role_value}" ]] || fail

  local network_name chain_volume
  # The production Explorer is intentionally still on the previous reviewed
  # Core pin while a new pin is proved in integration. Requiring both images
  # to match here makes every Core-pin roll-forward impossible. The candidate
  # image itself is pinned and verified above; production only supplies a
  # healthy read-only chain mount and must never be mutated by this gate.
  network_name=$(docker inspect "${production}" | jq -er '
    .[0].NetworkSettings.Networks | keys | select(length == 1) | .[0]') || fail
  chain_volume=$(docker inspect "${production}" | jq -er --arg dst "${chain_destination}" '
    [.[0].Mounts[] | select(.Destination == $dst and .RW == false and .Type == "volume")]
    | select(length == 1) | .[0].Name') || fail
  local -a explorer_cmd
  mapfile -t explorer_cmd < <(docker inspect "${production}" | jq -er '.[0].Config.Cmd[]') || fail
  ((${#explorer_cmd[@]} > 0)) || fail

  local current=''
  if [[ -f ${active_file} ]]; then
    IFS= read -r current <"${active_file}"
    [[ ${current} =~ ^qwertycoin-explorer-integration-[a-z0-9-]+$ ]] || fail
    docker inspect "${current}" >/dev/null 2>&1 || fail
    [[ $(docker inspect "${current}" --format '{{index .Config.Labels "org.qwertycoin.role"}}') == "${role_value}" ]] || fail
    local current_image current_revision
    current_image=$(docker inspect "${current}" --format '{{.Image}}') || fail
    current_revision=$(docker image inspect "${current_image}" \
      --format '{{index .Config.Labels "org.opencontainers.image.revision"}}') || fail
    valid_sha "${current_revision}" || fail
    if [[ ${current_revision} == "${explorer_sha}" \
        && $(docker inspect "${current}" --format '{{.State.Running}}') == true \
        && $(docker inspect "${current}" --format '{{.State.Health.Status}}') == healthy ]]; then
      printf 'ALREADY_CURRENT\n'
      return
    fi
  fi

  local volume_created=0
  if ! docker volume inspect "${INTEGRATION_DERIVED_VOLUME}" >/dev/null 2>&1; then
    docker volume create --label "${role_label}" "${INTEGRATION_DERIVED_VOLUME}" >/dev/null
    volume_created=1
  fi
  [[ $(docker volume inspect "${INTEGRATION_DERIVED_VOLUME}" \
    --format '{{index .Labels "org.qwertycoin.role"}}') == "${role_value}" ]] || fail
  if ((volume_created == 1)); then
    docker run --rm \
      --label "${role_label}" \
      --network none \
      --read-only \
      --cap-drop ALL \
      --cap-add CHOWN \
      --security-opt no-new-privileges:true \
      --user 0:0 \
      --entrypoint /bin/chown \
      --volume "${INTEGRATION_DERIVED_VOLUME}:/data" \
      "${image_id}" 101:101 /data >/dev/null
  fi

  local new_name="qwertycoin-explorer-integration-${explorer_sha:0:12}"
  local stamp rollback_name='' failed_name mutated=0
  stamp=$(date -u +%Y%m%dT%H%M%SZ)
  failed_name="${new_name}-failed-${stamp}"
  ! docker inspect "${new_name}" >/dev/null 2>&1 || fail
  if [[ -n ${current} ]]; then
    rollback_name="${current}-rollback-${stamp}"
    ! docker inspect "${rollback_name}" >/dev/null 2>&1 || fail
  fi

  local stale
  stale=$(docker ps -aq --filter "label=${role_label}" --filter status=exited)

  rollback() {
    local rc=$?
    trap - ERR
    set +e
    if ((mutated == 1)); then
      if docker inspect "${new_name}" >/dev/null 2>&1; then
        docker stop --time 30 "${new_name}" >/dev/null 2>&1
        docker rename "${new_name}" "${failed_name}" >/dev/null 2>&1
      fi
      if [[ -n ${rollback_name} ]] && docker inspect "${rollback_name}" >/dev/null 2>&1; then
        docker rename "${rollback_name}" "${current}" >/dev/null 2>&1
        docker start "${current}" >/dev/null 2>&1
      fi
    fi
    printf 'DEPLOY_ERROR\n' >&2
    exit "${rc}"
  }
  trap rollback ERR

  if [[ -n ${current} ]]; then
    docker stop --time 30 "${current}" >/dev/null
    docker rename "${current}" "${rollback_name}" >/dev/null
    mutated=1
  fi

  mutated=1
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
    --memory 1536m \
    --cpus 1.5 \
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
    --publish "${INTEGRATION_HOST_IP}:${INTEGRATION_HOST_PORT}:8081" \
    --volume "${chain_volume}:${chain_destination}:ro" \
    --volume "${INTEGRATION_DERIVED_VOLUME}:${derived_destination}" \
    "${image_id}" "${explorer_cmd[@]}" >/dev/null
  local ready=0 ready_body
  for _ in $(seq 1 300); do
    if ready_body=$(curl --fail --silent --show-error --max-time 5 \
        "http://${INTEGRATION_HOST_IP}:${INTEGRATION_HOST_PORT}/readyz" 2>/dev/null) \
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

  local version
  version=$(curl --fail --silent --show-error --max-time 10 \
    "http://${INTEGRATION_HOST_IP}:${INTEGRATION_HOST_PORT}/api/v1/version")
  jq -e --arg explorer "${explorer_sha}" --arg core "${core_revision}" '
    .status == "success"
    and .data.explorer_source_sha == $explorer
    and .data.qwc_source_sha == $core' >/dev/null <<<"${version}"

  local next_active="${active_file}.next.$$"
  mkdir -p "${state_directory}"
  printf '%s\n' "${new_name}" >"${next_active}"
  mv "${next_active}" "${active_file}"

  for stale_container in ${stale}; do
    [[ -n ${stale_container} ]] || continue
    docker rm "${stale_container}" >/dev/null 2>&1 || true
  done

  trap - ERR
  printf 'ROLLOUT_OK\n'
}

main() {
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
