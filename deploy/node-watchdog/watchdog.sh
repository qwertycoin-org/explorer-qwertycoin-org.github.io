#!/usr/bin/env bash
set -Eeuo pipefail

readonly EXPECTED_GENESIS="4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39"
readonly EXPECTED_CORE_IMAGE="sha256:7cf36ce1cb0ca4a83c4e2de5ae105811a43a6a9f44f4d2d65d2c42d916be0178"
readonly EXPECTED_APP_IMAGE="sha256:37596b75953eed94dbf64fb617500be347c3a6a0d02f85d8f6b0d570402e6741"
readonly CORE_CONTAINER="qwertycoin-mainnet"
readonly EXPLORER_CONTAINER="qwertycoin-explorer-ui-5ef68bd"
readonly GATEWAY_CONTAINER="qwertycoin-wallet-gateway-ui-5ef68bd"
readonly STATE_DIR="/workspace/state/qwc-node-watchdog"
readonly PAUSE_FILE="${STATE_DIR}/PAUSED"
readonly LOCK_FILE="${STATE_DIR}/repair.lock"

readonly -a HOSTS=(
  "seed-00.qwertycoin.org"
  "seed-01.qwertycoin.org"
  "seed-02.qwertycoin.org"
  "seed-03.qwertycoin.org"
)

mkdir -p "${STATE_DIR}"

container_name_for_role() {
  case "$1" in
    core) printf '%s\n' "${CORE_CONTAINER}" ;;
    explorer) printf '%s\n' "${EXPLORER_CONTAINER}" ;;
    gateway) printf '%s\n' "${GATEWAY_CONTAINER}" ;;
    *) return 1 ;;
  esac
}

expected_image_for_role() {
  case "$1" in
    core) printf '%s\n' "${EXPECTED_CORE_IMAGE}" ;;
    explorer|gateway) printf '%s\n' "${EXPECTED_APP_IMAGE}" ;;
    *) return 1 ;;
  esac
}

probe_host() {
  local host="$1"
  local raw marker

  if ! raw="$(ssh \
      -o BatchMode=yes \
      -o ConnectTimeout=8 \
      -o ConnectionAttempts=1 \
      -o LogLevel=ERROR \
      "${host}" \
      bash -s -- \
      "${EXPECTED_GENESIS}" \
      "${EXPECTED_CORE_IMAGE}" \
      "${EXPECTED_APP_IMAGE}" \
      "${CORE_CONTAINER}" \
      "${EXPLORER_CONTAINER}" \
      "${GATEWAY_CONTAINER}" <<'REMOTE'
set -Eeuo pipefail

expected_genesis="$1"
expected_core_image="$2"
expected_app_image="$3"
core_container="$4"
explorer_container="$5"
gateway_container="$6"

inspect_component() {
  local role="$1"
  local name="$2"
  local expected_image="$3"
  local inspect_json exists state health image restart_policy

  if ! inspect_json="$(docker inspect "${name}" 2>/dev/null)"; then
    jq -nc --arg role "${role}" --arg name "${name}" \
      '{role:$role,name:$name,exists:false,state:"missing",health:"none",image:"",restart_policy:""}'
    return
  fi

  exists=true
  state="$(jq -r '.[0].State.Status // "unknown"' <<<"${inspect_json}")"
  health="$(jq -r '.[0].State.Health.Status // "none"' <<<"${inspect_json}")"
  image="$(jq -r '.[0].Image // ""' <<<"${inspect_json}")"
  restart_policy="$(jq -r '.[0].HostConfig.RestartPolicy.Name // ""' <<<"${inspect_json}")"

  jq -nc \
    --arg role "${role}" \
    --arg name "${name}" \
    --arg state "${state}" \
    --arg health "${health}" \
    --arg image "${image}" \
    --arg restart_policy "${restart_policy}" \
    '{role:$role,name:$name,exists:true,state:$state,health:$health,image:$image,restart_policy:$restart_policy}'
}

core="$(inspect_component core "${core_container}" "${expected_core_image}")"
explorer="$(inspect_component explorer "${explorer_container}" "${expected_app_image}")"
gateway="$(inspect_component gateway "${gateway_container}" "${expected_app_image}")"

core_rpc_ok=false
core_genesis_ok=false
core_height=null
core_top_hash=""
core_peers=null
core_info=''
genesis_info=''

if [[ "$(jq -r '.state' <<<"${core}")" == "running" ]]; then
  core_info="$(curl -fsS --max-time 6 \
    -H 'Content-Type: application/json' \
    --data '{"jsonrpc":"2.0","id":"qwc-watchdog","method":"get_info"}' \
    http://127.0.0.1:8197/json_rpc 2>/dev/null || true)"
  if jq -e '.result.status == "OK" and .result.mainnet == true and .result.nettype == "mainnet"' \
      >/dev/null 2>&1 <<<"${core_info}"; then
    core_rpc_ok=true
    core_height="$(jq -r '.result.height' <<<"${core_info}")"
    core_top_hash="$(jq -r '.result.top_block_hash // ""' <<<"${core_info}")"
    core_peers="$(jq -r '(.result.incoming_connections_count // 0) + (.result.outgoing_connections_count // 0)' <<<"${core_info}")"
  fi

  genesis_info="$(curl -fsS --max-time 6 \
    -H 'Content-Type: application/json' \
    --data '{"jsonrpc":"2.0","id":"qwc-watchdog","method":"get_block_header_by_height","params":{"height":0}}' \
    http://127.0.0.1:8197/json_rpc 2>/dev/null || true)"
  if jq -e --arg genesis "${expected_genesis}" \
      '.result.status == "OK" and .result.block_header.hash == $genesis' \
      >/dev/null 2>&1 <<<"${genesis_info}"; then
    core_genesis_ok=true
  fi
fi

explorer_endpoint_ok=false
explorer_liveness_ok=false
explorer_body=''
if [[ "$(jq -r '.state' <<<"${explorer}")" == "running" ]]; then
  if curl -fsS --max-time 5 http://127.0.0.1:29990/healthz \
      | jq -e '.status == "success" and .data.process == "running"' >/dev/null 2>&1; then
    explorer_liveness_ok=true
  fi
  explorer_body="$(curl -fsS --max-time 8 http://127.0.0.1:29990/readyz 2>/dev/null || true)"
  if jq -e --arg genesis "${expected_genesis}" '
      .status == "success" and
      .data.identity.compatible == true and
      .data.identity.genesis_matches == true and
      .data.identity.genesis_hash == $genesis and
      .data.identity.rpc_db_anchor_matches == true and
      .data.wallet_rpc.status == "ready"
    ' >/dev/null 2>&1 <<<"${explorer_body}"; then
    explorer_endpoint_ok=true
  fi
fi

gateway_endpoint_ok=false
gateway_liveness_ok=false
gateway_body=''
if [[ "$(jq -r '.state' <<<"${gateway}")" == "running" ]]; then
  if curl -fsS --max-time 5 http://127.0.0.1:29985/healthz \
      | jq -e '.status == "success" and .data.process == "running"' >/dev/null 2>&1; then
    gateway_liveness_ok=true
  fi
  gateway_body="$(curl -fsS --max-time 8 \
    -H 'Content-Type: application/json' \
    --data '{"jsonrpc":"2.0","id":"qwc-watchdog","method":"get_info"}' \
    http://127.0.0.1:29985/qwc-rpc/json_rpc 2>/dev/null || true)"
  if jq -e '.result.status == "OK" and .result.mainnet == true and .result.nettype == "mainnet"' \
      >/dev/null 2>&1 <<<"${gateway_body}"; then
    gateway_endpoint_ok=true
  fi
fi

core="$(jq \
  --argjson rpc_ok "${core_rpc_ok}" \
  --argjson genesis_ok "${core_genesis_ok}" \
  --argjson height "${core_height}" \
  --arg top_hash "${core_top_hash}" \
  --argjson peers "${core_peers}" \
  '. + {rpc_ok:$rpc_ok,genesis_ok:$genesis_ok,height:$height,top_hash:$top_hash,peers:$peers}' <<<"${core}")"
explorer="$(jq \
  --argjson liveness_ok "${explorer_liveness_ok}" \
  --argjson endpoint_ok "${explorer_endpoint_ok}" \
  '. + {liveness_ok:$liveness_ok,endpoint_ok:$endpoint_ok}' <<<"${explorer}")"
gateway="$(jq \
  --argjson liveness_ok "${gateway_liveness_ok}" \
  --argjson endpoint_ok "${gateway_endpoint_ok}" \
  '. + {liveness_ok:$liveness_ok,endpoint_ok:$endpoint_ok}' <<<"${gateway}")"

faults='[]'
add_fault() {
  local severity="$1"
  local role="$2"
  local reason="$3"
  faults="$(jq -c \
    --arg severity "${severity}" \
    --arg role "${role}" \
    --arg reason "${reason}" \
    '. + [{severity:$severity,role:$role,reason:$reason}]' <<<"${faults}")"
}

for component in "${core}" "${explorer}" "${gateway}"; do
  role="$(jq -r '.role' <<<"${component}")"
  exists="$(jq -r '.exists' <<<"${component}")"
  state="$(jq -r '.state' <<<"${component}")"
  image="$(jq -r '.image' <<<"${component}")"
  restart_policy="$(jq -r '.restart_policy' <<<"${component}")"
  expected_image="${expected_app_image}"
  [[ "${role}" == "core" ]] && expected_image="${expected_core_image}"

  if [[ "${exists}" != "true" ]]; then
    add_fault invariant "${role}" container_missing
  elif [[ "${image}" != "${expected_image}" ]]; then
    add_fault invariant "${role}" unexpected_image
  elif [[ "${restart_policy}" != "unless-stopped" ]]; then
    add_fault invariant "${role}" unexpected_restart_policy
  elif [[ "${state}" != "running" ]]; then
    add_fault service "${role}" container_not_running
  fi
done

if [[ "$(jq -r '.exists' <<<"${core}")" == "true" && "$(jq -r '.state' <<<"${core}")" == "running" ]]; then
  if [[ "${core_rpc_ok}" != "true" ]]; then
    add_fault service core rpc_unavailable
  elif [[ "${core_genesis_ok}" != "true" ]]; then
    add_fault invariant core genesis_mismatch
  elif [[ "${core_peers}" == "0" ]]; then
    add_fault alert network no_peers
  fi
fi

if [[ "$(jq -r '.exists' <<<"${explorer}")" == "true" && "$(jq -r '.state' <<<"${explorer}")" == "running" ]]; then
  if [[ "${explorer_liveness_ok}" != "true" ]]; then
    add_fault service explorer liveness_failed
  elif [[ "${explorer_endpoint_ok}" != "true" ]]; then
    add_fault alert explorer readiness_failed
  fi
fi

if [[ "$(jq -r '.exists' <<<"${gateway}")" == "true" && "$(jq -r '.state' <<<"${gateway}")" == "running" ]]; then
  if [[ "${gateway_liveness_ok}" != "true" ]]; then
    add_fault service gateway liveness_failed
  elif [[ "${gateway_endpoint_ok}" != "true" ]]; then
    add_fault alert gateway wallet_rpc_failed
  fi
fi

result="$(jq -nc \
  --argjson core "${core}" \
  --argjson explorer "${explorer}" \
  --argjson gateway "${gateway}" \
  --argjson faults "${faults}" \
  '{reachable:true,components:{core:$core,explorer:$explorer,gateway:$gateway},faults:$faults}')"
printf 'QWC_WATCHDOG_JSON\t%s\n' "${result}"
REMOTE
  )"; then
    jq -nc --arg host "${host}" \
      '{host:$host,reachable:false,components:{},faults:[{severity:"service",role:"host",reason:"ssh_unreachable"}]}'
    return
  fi

  marker="$(sed -n 's/^QWC_WATCHDOG_JSON[[:space:]]//p' <<<"${raw}" | tail -n 1)"
  if ! jq -e . >/dev/null 2>&1 <<<"${marker}"; then
    jq -nc --arg host "${host}" \
      '{host:$host,reachable:false,components:{},faults:[{severity:"service",role:"host",reason:"invalid_probe_output"}]}'
    return
  fi

  jq -c --arg host "${host}" '. + {host:$host}' <<<"${marker}"
}

probe_all() {
  local nodes='[]'
  local host node

  for host in "${HOSTS[@]}"; do
    node="$(probe_host "${host}")"
    nodes="$(jq -c --argjson node "${node}" '. + [$node]' <<<"${nodes}")"
  done

  jq -nc --argjson nodes "${nodes}" '
    {
      paused:false,
      checked_at:(now | floor),
      nodes:$nodes,
      faults:[
        $nodes[] as $node |
        $node.faults[] |
        . + {host:$node.host,key:($node.host + ":" + .role + ":" + .reason)}
      ]
    }
    | .healthy = (.faults | length == 0)
  '
}

probe() {
  if [[ -e "${PAUSE_FILE}" ]]; then
    jq -nc '{paused:true,healthy:true,checked_at:(now | floor),nodes:[],faults:[]}'
    return
  fi
  probe_all
}

repair() {
  local host="${1:-}"
  local role="${2:-}"
  local snapshot target_fault severity reason node_state component_state expected_image container action timeout healthy_others attempt refreshed

  if [[ -z "${host}" || -z "${role}" ]]; then
    jq -nc '{status:"refused",reason:"missing_target"}'
    return 2
  fi
  if [[ ! " ${HOSTS[*]} " =~ [[:space:]]${host}[[:space:]] ]]; then
    jq -nc --arg host "${host}" '{status:"refused",reason:"unknown_host",host:$host}'
    return 2
  fi
  if ! container="$(container_name_for_role "${role}")"; then
    jq -nc --arg role "${role}" '{status:"refused",reason:"unknown_role",role:$role}'
    return 2
  fi
  expected_image="$(expected_image_for_role "${role}")"

  exec 9>"${LOCK_FILE}"
  if ! flock -n 9; then
    jq -nc --arg host "${host}" --arg role "${role}" \
      '{status:"skipped",reason:"repair_already_running",host:$host,role:$role}'
    return
  fi
  if [[ -e "${PAUSE_FILE}" ]]; then
    jq -nc --arg host "${host}" --arg role "${role}" \
      '{status:"skipped",reason:"maintenance_paused",host:$host,role:$role}'
    return
  fi

  snapshot="$(probe_all)"
  target_fault="$(jq -c --arg host "${host}" --arg role "${role}" \
    '[.faults[] | select(.host == $host and .role == $role)] | first // empty' <<<"${snapshot}")"
  if [[ -z "${target_fault}" ]]; then
    jq -nc --arg host "${host}" --arg role "${role}" \
      '{status:"recovered_without_action",host:$host,role:$role}'
    return
  fi

  severity="$(jq -r '.severity' <<<"${target_fault}")"
  reason="$(jq -r '.reason' <<<"${target_fault}")"
  if [[ "${severity}" != "service" ]]; then
    jq -nc --arg host "${host}" --arg role "${role}" --arg severity "${severity}" --arg reason "${reason}" \
      '{status:"alert_only",host:$host,role:$role,severity:$severity,reason:$reason}'
    return
  fi
  if [[ "${role}" == "host" ]]; then
    jq -nc --arg host "${host}" --arg reason "${reason}" \
      '{status:"alert_only",host:$host,role:"host",severity:"service",reason:$reason}'
    return
  fi

  node_state="$(jq -c --arg host "${host}" '.nodes[] | select(.host == $host)' <<<"${snapshot}")"
  component_state="$(jq -c --arg role "${role}" '.components[$role]' <<<"${node_state}")"
  if [[ "$(jq -r '.exists' <<<"${component_state}")" != "true" ||
        "$(jq -r '.image' <<<"${component_state}")" != "${expected_image}" ||
        "$(jq -r '.restart_policy' <<<"${component_state}")" != "unless-stopped" ]]; then
    jq -nc --arg host "${host}" --arg role "${role}" \
      '{status:"refused",reason:"invariant_changed_before_repair",host:$host,role:$role}'
    return
  fi

  if [[ "${role}" == "core" ]]; then
    healthy_others="$(jq -r --arg host "${host}" --arg genesis "${EXPECTED_GENESIS}" '
      [
        .nodes[] |
        select(.host != $host) |
        select(.reachable == true) |
        select(.components.core.rpc_ok == true) |
        select(.components.core.genesis_ok == true)
      ] | length
    ' <<<"${snapshot}")"
    if (( healthy_others < 2 )); then
      jq -nc --arg host "${host}" --argjson healthy_others "${healthy_others}" \
        '{status:"refused",reason:"insufficient_healthy_core_quorum",host:$host,role:"core",healthy_other_cores:$healthy_others}'
      return
    fi
  else
    if [[ "$(jq -r '.components.core.rpc_ok' <<<"${node_state}")" != "true" ||
          "$(jq -r '.components.core.genesis_ok' <<<"${node_state}")" != "true" ]]; then
      jq -nc --arg host "${host}" --arg role "${role}" \
        '{status:"refused",reason:"local_core_not_healthy",host:$host,role:$role}'
      return
    fi
  fi

  if [[ "$(jq -r '.state' <<<"${component_state}")" == "running" ]]; then
    action="restart"
    timeout=30
    [[ "${role}" == "core" ]] && timeout=120
    ssh -o BatchMode=yes -o ConnectTimeout=8 -o ConnectionAttempts=1 -o LogLevel=ERROR \
      "${host}" docker restart --time "${timeout}" "${container}" >/dev/null
  else
    action="start"
    ssh -o BatchMode=yes -o ConnectTimeout=8 -o ConnectionAttempts=1 -o LogLevel=ERROR \
      "${host}" docker start "${container}" >/dev/null
  fi

  for attempt in $(seq 1 18); do
    sleep 10
    refreshed="$(probe_host "${host}")"
    if ! jq -e --arg role "${role}" \
      '[.faults[] | select(.role == $role and (.severity == "service" or .severity == "invariant"))] | length == 0' \
      >/dev/null <<<"${refreshed}"; then
      continue
    fi

    jq -nc \
      --arg status repaired \
      --arg host "${host}" \
      --arg role "${role}" \
      --arg reason "${reason}" \
      --arg action "${action}" \
      --argjson attempts "${attempt}" \
      '{status:$status,host:$host,role:$role,reason:$reason,action:$action,verified_after_seconds:($attempts * 10)}'
    return
  done

  jq -nc --arg host "${host}" --arg role "${role}" --arg reason "${reason}" --arg action "${action}" \
    '{status:"repair_failed",host:$host,role:$role,reason:$reason,action:$action}'
  return 1
}

usage() {
  printf 'usage: %s probe | repair HOST ROLE\n' "$0" >&2
}

case "${1:-}" in
  probe)
    probe
    ;;
  repair)
    shift
    repair "${1:-}" "${2:-}"
    ;;
  *)
    usage
    exit 2
    ;;
esac
