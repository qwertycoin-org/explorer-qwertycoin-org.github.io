file(READ "${SOURCE_DIR}/main.cpp" MAIN_SOURCE)
file(READ "${SOURCE_DIR}/src/templates/index2.html" OVERVIEW_TEMPLATE)
file(READ "${SOURCE_DIR}/src/templates/block.html" BLOCK_TEMPLATE)
file(READ "${SOURCE_DIR}/src/templates/css/style.css" STYLE_SOURCE)
file(READ "${SOURCE_DIR}/src/templates/assets/epose-status.js" EPOSE_STATUS_SOURCE)
file(READ "${SOURCE_DIR}/src/templates/header.html" HEADER_TEMPLATE)
file(READ "${SOURCE_DIR}/src/templates/partials/tx_details.html" TX_TEMPLATE)
file(READ "${SOURCE_DIR}/src/page.h" PAGE_SOURCE)
file(READ "${SOURCE_DIR}/deploy/explorer.qwertycoin.org.nginx.conf" NGINX_SOURCE)
file(READ "${SOURCE_DIR}/src/wallet_rpc_policy.h" RPC_POLICY_SOURCE)
file(READ "${SOURCE_DIR}/src/rpccalls.h" RPC_CALLS_HEADER)
file(READ "${SOURCE_DIR}/src/rpccalls.cpp" RPC_CALLS_SOURCE)
file(READ "${SOURCE_DIR}/src/CmdLineOptions.cpp" OPTIONS_SOURCE)
file(READ "${SOURCE_DIR}/docker-compose.production.yml" PRODUCTION_COMPOSE)

set(FORBIDDEN_ROUTES
    "CROW_ROUTE(app, \"/myoutputs\""
    "CROW_ROUTE(app, \"/prove\""
    "CROW_ROUTE(app, \"/api/outputs\""
    "CROW_ROUTE(app, \"/api/outputsblocks\""
    "CROW_ROUTE(app, \"/rawkeyimgs\""
    "CROW_ROUTE(app, \"/rawoutputkeys\"")
foreach(ROUTE IN LISTS FORBIDDEN_ROUTES)
    string(FIND "${MAIN_SOURCE}" "${ROUTE}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Retired secret route was registered: ${ROUTE}")
    endif()
endforeach()

foreach(REQUIRED_EPOSE_TEXT
        "Advertised endpoint"
        "Service period"
        "Qualification · Epoch"
        "Reward eligibility · Epoch"
        "Technical identity"
        "column-help"
        "EPoSe service protocol version"
        "Endpoint descriptor schema version"
        "Persistent node ID (stable)"
        "Service verification key (current epoch)"
        "Endpoint descriptor hash"
        "Preview not exposed by Core"
        "EPoSe data loaded"
        "Qualification in this epoch determines reward eligibility in the next epoch."
        "Qualification closes after block"
        "Signed endpoint advertisement"
        "not a reachability check"
        "/assets/epose-status.js?v={{asset_version}}"
        "endpointName.textContent"
        "code.textContent")
    string(FIND "${OVERVIEW_TEMPLATE}" "${REQUIRED_EPOSE_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Readable EPoSE presentation contract is missing: ${REQUIRED_EPOSE_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_EPOSE_STATUS_TEXT
        "qualificationAnchorDepth: 60"
        "Pending"
        "Qualified"
        "Not qualified"
        "Not participating"
        "Unavailable"
        "Registration active"
        "Qualification has not been finalized"
        "snapshotContext"
        "rewardHeight === tipHeight + 1"
        "rewardsEpoch + 1 === currentEpoch"
        "currentFinal: nodesObserved > infoObserved"
        "expires at start of Epoch"
        "qualification_availability !== \"current\""
        "source_qualification_availability !== \"finalized\""
        "tipHeight >= closeHeight"
        "currentEpoch !== qualificationEpoch")
    string(FIND "${EPOSE_STATUS_SOURCE}" "${REQUIRED_EPOSE_STATUS_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Shared EPoSe status derivation is missing: ${REQUIRED_EPOSE_STATUS_TEXT}")
    endif()
endforeach()

foreach(FORBIDDEN_EPOSE_STATUS_TEXT
        "Pending means participation is confirmed"
        "Qualification is not final yet"
        "Sequence \" + node.descriptor_sequence + \", epochs")
    string(FIND "${OVERVIEW_TEMPLATE}${EPOSE_STATUS_SOURCE}" "${FORBIDDEN_EPOSE_STATUS_TEXT}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Ambiguous EPoSe status presentation remains: ${FORBIDDEN_EPOSE_STATUS_TEXT}")
    endif()
endforeach()

string(FIND "${OVERVIEW_TEMPLATE}"
    "qualification.phase === \"closed\" && snapshots.currentFinal !== true"
    CLOSED_SNAPSHOT_GUARD)
if(CLOSED_SNAPSHOT_GUARD EQUAL -1)
    message(FATAL_ERROR "Closed qualification metric must reject an unanchored node snapshot")
endif()

string(FIND "${OVERVIEW_TEMPLATE}" "EPoSE" LEGACY_PUBLIC_EPOSE_SPELLING)
if(NOT LEGACY_PUBLIC_EPOSE_SPELLING EQUAL -1)
    message(FATAL_ERROR "Public explorer UI must spell EPoSe consistently")
endif()

foreach(FORBIDDEN_EPOSE_PRESENTATION_TEXT
        "Independent online check"
        "Not exposed by Core"
        "Current-epoch qualification is still evolving"
        "one common atomic block anchor"
        "Qwertycoin v2 mainnet"
        "Final genesis is active"
        "validating read-only observer")
    string(FIND "${OVERVIEW_TEMPLATE}" "${FORBIDDEN_EPOSE_PRESENTATION_TEXT}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Removed explorer presentation text remains: ${FORBIDDEN_EPOSE_PRESENTATION_TEXT}")
    endif()
endforeach()

foreach(FORBIDDEN_EPOSE_TEXT
        "Stable identity"
        "Current service key"
        "Unanchored observer data"
        ">Reward preview<"
        ">Reachability<")
    string(FIND "${OVERVIEW_TEMPLATE}" "${FORBIDDEN_EPOSE_TEXT}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Cryptic EPoSE presentation text remains: ${FORBIDDEN_EPOSE_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_EPOSE_RPC_TEXT
        "get_epose_service_endpoint_v2"
        "/get_epose_service_endpoint_v2"
        "descriptor_hash")
    string(FIND "${RPC_CALLS_SOURCE}" "${REQUIRED_EPOSE_RPC_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "EPoSE endpoint lookup contract is missing: ${REQUIRED_EPOSE_RPC_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_EPOSE_API_TEXT
        "valid_epose_advertised_endpoint"
        "advertised_endpoint"
        "descriptor_version"
        "service_version"
        "signed_descriptor_lookup"
        "core-validated signed descriptor")
    string(FIND "${PAGE_SOURCE}" "${REQUIRED_EPOSE_API_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "EPoSE endpoint adapter contract is missing: ${REQUIRED_EPOSE_API_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_GATEWAY_DEPLOYMENT_TEXT
        "qwertycoin-wallet-gateway:"
        "QWC_WALLET_GATEWAY_IPV4"
        "QWC_WALLET_GATEWAY_DERIVED_PATH"
        "http://127.0.0.1:8081/readyz")
    string(FIND "${PRODUCTION_COMPOSE}" "${REQUIRED_GATEWAY_DEPLOYMENT_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Hardened wallet gateway deployment is missing: ${REQUIRED_GATEWAY_DEPLOYMENT_TEXT}")
    endif()
endforeach()

string(FIND "${STYLE_SOURCE}" ".blocks-table .optional-block-field { display: none; }" MOBILE_OPTIONAL_FIELDS)
if(NOT MOBILE_OPTIONAL_FIELDS EQUAL -1)
    message(FATAL_ERROR "Mobile block cards must keep every public block field reachable")
endif()
string(FIND "${STYLE_SOURCE}" ".blocks-table td:nth-child(7)" HIDDEN_BLOCK_HASH)
if(NOT HIDDEN_BLOCK_HASH EQUAL -1)
    message(FATAL_ERROR "Mobile block hash must not be hidden by column position")
endif()

foreach(REQUIRED_ASSET
        "src/templates/assets/qwertycoin-mark.svg"
        "src/templates/assets/favicon.svg"
        "src/templates/assets/favicon.ico"
        "src/templates/assets/favicon-16x16.png"
        "src/templates/assets/favicon-32x32.png"
        "src/templates/assets/favicon-192x192.png"
        "src/templates/assets/apple-touch-icon.png"
        "src/templates/assets/epose-status.js"
        "src/templates/assets/fonts/archivo-latin-900.woff2"
        "src/templates/assets/fonts/inter-latin-400.woff2"
        "src/templates/assets/fonts/inter-latin-600.woff2")
    if(NOT EXISTS "${SOURCE_DIR}/${REQUIRED_ASSET}")
        message(FATAL_ERROR "Required local brand asset is missing: ${REQUIRED_ASSET}")
    endif()
endforeach()

if(DEFINED BINARY_DIR)
    foreach(ASSET_PATH
            "assets/qwertycoin-mark.svg"
            "assets/favicon.svg"
            "assets/favicon.ico"
            "assets/favicon-16x16.png"
            "assets/favicon-32x32.png"
            "assets/favicon-192x192.png"
            "assets/apple-touch-icon.png"
            "assets/epose-status.js"
            "assets/fonts/archivo-latin-900.woff2"
            "assets/fonts/inter-latin-400.woff2"
            "assets/fonts/inter-latin-600.woff2")
        set(SOURCE_ASSET "${SOURCE_DIR}/src/templates/${ASSET_PATH}")
        set(RUNTIME_ASSET "${BINARY_DIR}/templates/${ASSET_PATH}")
        if(NOT EXISTS "${RUNTIME_ASSET}")
            message(FATAL_ERROR "Configured runtime asset is missing: ${RUNTIME_ASSET}")
        endif()
        file(SHA256 "${SOURCE_ASSET}" SOURCE_ASSET_SHA256)
        file(SHA256 "${RUNTIME_ASSET}" RUNTIME_ASSET_SHA256)
        if(NOT SOURCE_ASSET_SHA256 STREQUAL RUNTIME_ASSET_SHA256)
            message(FATAL_ERROR "Configured runtime asset differs from source: ${ASSET_PATH}")
        endif()
    endforeach()
endif()

foreach(REQUIRED_BRAND_TEXT
        "/assets/qwertycoin-mark.svg"
        "/assets/style.css?v={{asset_version}}"
        "/assets/favicon.svg"
        "/favicon.ico"
        "QWERTYCOIN"
        "EXPLORER"
        "data-section=\"service-nodes\""
        "aria-current"
        "qwc-theme"
        "if(t!==\"light\"&&t!==\"dark\")t=\"light\""
        "Close explorer navigation"
        "Copy failed")
    string(FIND "${HEADER_TEMPLATE}" "${REQUIRED_BRAND_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Explorer brand/accessibility contract is missing: ${REQUIRED_BRAND_TEXT}")
    endif()
endforeach()

string(FIND "${HEADER_TEMPLATE}" "prefers-color-scheme: dark" SYSTEM_THEME_DEFAULT)
if(NOT SYSTEM_THEME_DEFAULT EQUAL -1)
    message(FATAL_ERROR "Explorer must default to light rather than inheriting the operating-system theme")
endif()

foreach(REQUIRED_DESIGN_TEXT
        "--page: #F5F1E7"
        "--surface: #FFFDF7"
        "--gold: #FFAF00"
        "--violet: #7952FF"
        "font-family: \"Archivo\""
        "font-family: \"Inter\""
        "min-height: 82px"
        "backdrop-filter: blur(16px)"
        "box-shadow: 4px 4px 0 var(--ink)"
        "grid-template-columns: 42px auto"
        "letter-spacing: 0.16em"
        ".brand-name { font-size: 0.94rem; }"
        "@media (max-width: 1200px)"
        "prefers-reduced-motion")
    string(FIND "${STYLE_SOURCE}" "${REQUIRED_DESIGN_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Explorer design-system contract is missing: ${REQUIRED_DESIGN_TEXT}")
    endif()
endforeach()

foreach(FORBIDDEN_LEGACY_STYLE "Montserrat" "Open Sans" "#071327")
    string(FIND "${STYLE_SOURCE}" "${FORBIDDEN_LEGACY_STYLE}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Legacy explorer styling remains: ${FORBIDDEN_LEGACY_STYLE}")
    endif()
endforeach()

foreach(REQUIRED_ASSET_ROUTE
        "CROW_ROUTE(app, \"/favicon.ico\")"
        "CROW_ROUTE(app, \"/assets/<string>\")"
        "epose-status.js"
        "text/javascript; charset=utf-8"
        "CROW_ROUTE(app, \"/assets/fonts/<string>\")"
        "./templates/css/style.css"
        "text/css; charset=utf-8"
        "max-age=31536000, immutable"
        "font/woff2"
        "image/svg+xml")
    string(FIND "${MAIN_SOURCE}" "${REQUIRED_ASSET_ROUTE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Local asset route is missing: ${REQUIRED_ASSET_ROUTE}")
    endif()
endforeach()

foreach(FORBIDDEN_INLINE_STYLE
        "<style type=\"text/css\">"
        "{{#css_styles}}{{/css_styles}}")
    string(FIND "${HEADER_TEMPLATE}" "${FORBIDDEN_INLINE_STYLE}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Shared stylesheet must not be duplicated inline: ${FORBIDDEN_INLINE_STYLE}")
    endif()
endforeach()

foreach(REQUIRED_EDGE_HEADER_TEXT
        "proxy_hide_header Content-Security-Policy"
        "proxy_hide_header X-Frame-Options"
        "proxy_hide_header X-Content-Type-Options"
        "proxy_hide_header Referrer-Policy"
        "proxy_hide_header Permissions-Policy"
        "Strict-Transport-Security \"max-age=31536000\"")
    string(FIND "${NGINX_SOURCE}" "${REQUIRED_EDGE_HEADER_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Public edge-header contract is missing: ${REQUIRED_EDGE_HEADER_TEXT}")
    endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" CMAKE_SOURCE)
string(FIND "${CMAKE_SOURCE}" "src/templates/assets/fonts" FONT_COPY_RULE)
if(FONT_COPY_RULE EQUAL -1)
    message(FATAL_ERROR "Local fonts are not copied into the runtime template tree")
endif()
file(READ "${SOURCE_DIR}/cmake/MyUtils.cmake" CMAKE_UTILS_SOURCE)
string(FIND "${CMAKE_UTILS_SOURCE}" "COPYONLY" BINARY_COPY_RULE)
if(BINARY_COPY_RULE EQUAL -1)
    message(FATAL_ERROR "Runtime assets must be copied byte-exactly")
endif()

foreach(REQUIRED_REFRESH_TEXT
        "setTimeout(refresh, ms)"
        "new AbortController()"
        "document.hidden"
        "Math.min(delay * 2, 120000)"
        "var refreshBlockRows = {{#is_page_zero}}true{{/is_page_zero}}{{^is_page_zero}}false{{/is_page_zero}}"
        "if (refreshBlockRows) renderBlocks(data.blocks)"
        "renderMempool(data.mempool)")
    string(FIND "${OVERVIEW_TEMPLATE}" "${REQUIRED_REFRESH_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Bounded dashboard refresh contract is missing: ${REQUIRED_REFRESH_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_BLOCK_LIST_TEXT
        "data-label=\"Difficulty\""
        "data-label=\"Block hash\"><a class=\"hash-link\" href=\"/block/{{hash}}\""
        "cell(\"Difficulty\", block.difficulty)"
        "hashLink.href = \"/block/\" + block.hash")
    string(FIND "${OVERVIEW_TEMPLATE}" "${REQUIRED_BLOCK_LIST_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Recent-block field/link contract is missing: ${REQUIRED_BLOCK_LIST_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_BLOCK_DATA_TEXT
        "{\"difficulty\", blk_difficulty}"
        "{\"difficulty\", core_storage->get_db().get_block_difficulty(height).str()}")
    string(FIND "${PAGE_SOURCE}" "${REQUIRED_BLOCK_DATA_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Recent-block difficulty data is missing: ${REQUIRED_BLOCK_DATA_TEXT}")
    endif()
endforeach()

string(FIND "${OPTIONS_SOURCE}" "daemon-url,d\", value<string>()->default_value" DAEMON_DEFAULT)
if(NOT DAEMON_DEFAULT EQUAL -1)
    message(FATAL_ERROR "Daemon URL must not silently default to an administrative listener")
endif()

foreach(REQUIRED_RPC_EDGE_TEXT
        "restricted daemon listener"
        "location /qwc-rpc/"
        "proxy_pass http://qwertycoin_wallet_rpc_backend"
        "limit_req zone=qwc_wallet_reads"
        "limit_req zone=qwc_wallet_submits"
        "location = /ha/readyz"
        "auth_request /_qwc_wallet_readyz"
        "proxy_pass http://qwertycoin_wallet_rpc_backend/readyz"
        "map $http_origin $qwc_wallet_cors_origin"
        "\"https://wallet.qwertycoin.org\" $http_origin"
        "\\.pages\\.dev$ $http_origin"
        "if ($request_method = OPTIONS)"
        "add_header Access-Control-Allow-Origin $qwc_wallet_cors_origin always"
        "add_header Access-Control-Allow-Methods \"POST, OPTIONS\" always"
        "add_header Access-Control-Allow-Headers \"Content-Type\" always"
        "add_header Vary \"Origin\" always")
    string(FIND "${NGINX_SOURCE}" "${REQUIRED_RPC_EDGE_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Restricted wallet RPC edge contract is missing: ${REQUIRED_RPC_EDGE_TEXT}")
    endif()
endforeach()

foreach(FORBIDDEN_RPC_CORS_TEXT
        "Access-Control-Allow-Origin \"*\""
        "Access-Control-Allow-Origin $http_origin")
    string(FIND "${NGINX_SOURCE}" "${FORBIDDEN_RPC_CORS_TEXT}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Wallet RPC CORS must not reflect arbitrary origins: ${FORBIDDEN_RPC_CORS_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_RPC_POLICY_TEXT
        "getblocks.bin"
        "get_outs.bin"
        "send_raw_transaction"
        "get_info"
        "get_output_histogram"
        "wallet_rpc_path_retry_safe"
        "wallet_rpc_policy_result::forbidden")
    string(FIND "${RPC_POLICY_SOURCE}" "${REQUIRED_RPC_POLICY_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Parsed wallet RPC policy contract is missing: ${REQUIRED_RPC_POLICY_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_RPC_RECOVERY_TEXT
        "Restricted wallet RPC probe is unavailable or incompatible"
        "invoke_with_reconnect")
    string(FIND "${MAIN_SOURCE}${RPC_CALLS_HEADER}" "${REQUIRED_RPC_RECOVERY_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "RPC recovery/readiness contract is missing: ${REQUIRED_RPC_RECOVERY_TEXT}")
    endif()
endforeach()

string(FIND "${NGINX_SOURCE}" "location ^~ /qwc-rpc/" GENERIC_RPC_BLOCK)
if(NOT GENERIC_RPC_BLOCK EQUAL -1)
    message(FATAL_ERROR "The wallet RPC path was replaced by a generic prefix handler")
endif()

foreach(REQUIRED_TEXT
        "{\"qualified_count\", info.qualified_count}"
        "source_qualified_service_keys.count(node.service_public_key) == 1"
        "{\"source_qualification_epoch\", rewards.epoch}"
        "{\"qualified_for_current_epoch\", node.qualified}"
        "get_epose_block_reward"
        "make_epose_reward_view"
        "confidential_amounts ? \"confidential\" : \"public\""
        "{\"rpc_db_anchor_matches\", anchor_matches}"
        "MAINNET_FINAL_GENESIS_HASH_V2"
        "MAINNET_FINAL_PARAMETER_SET_HASH_V2")
    string(FIND "${PAGE_SOURCE}" "${REQUIRED_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Required fail-closed correctness contract is missing: ${REQUIRED_TEXT}")
    endif()
endforeach()

string(FIND "${PAGE_SOURCE}" "{\"qualified_for_source_epoch\", node.qualified}" ALIASED_QUALIFICATION)
if(NOT ALIASED_QUALIFICATION EQUAL -1)
    message(FATAL_ERROR "Current and finalized reward-source qualification were aliased")
endif()

foreach(REQUIRED_BLOCK_REWARD_TEXT
        "Miner / pool payout"
        "EPoSe service payout"
        "source epoch"
        "The explorer does not infer recipients from output position")
    string(FIND "${BLOCK_TEMPLATE}" "${REQUIRED_BLOCK_REWARD_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Canonical block reward presentation is missing: ${REQUIRED_BLOCK_REWARD_TEXT}")
    endif()
endforeach()

foreach(FORBIDDEN_LAUNCH_TEXT
        "MAINNET_REHEARSAL_GENESIS_HASH_V2"
        "MAINNET_REHEARSAL_PARAMETER_SET_HASH_V2"
        "planned to be reset before final launch")
    string(FIND "${PAGE_SOURCE}${OVERVIEW_TEMPLATE}" "${FORBIDDEN_LAUNCH_TEXT}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Legacy rehearsal identity remains in the public explorer: ${FORBIDDEN_LAUNCH_TEXT}")
    endif()
endforeach()

string(FIND "${PAGE_SOURCE}" "current_network_info.current = true" CURRENT_OVERRIDE)
if(NOT CURRENT_OVERRIDE EQUAL -1)
    message(FATAL_ERROR "A cached RPC failure can be overwritten as current")
endif()

string(FIND "${PAGE_SOURCE}" "show_block(uint64_t _blk_height)" BLOCK_VIEW_START)
string(FIND "${PAGE_SOURCE}" "show_block(string _blk_hash)" BLOCK_VIEW_END)
if(BLOCK_VIEW_START EQUAL -1 OR BLOCK_VIEW_END EQUAL -1 OR BLOCK_VIEW_END LESS BLOCK_VIEW_START)
    message(FATAL_ERROR "Could not isolate ordinary block view for request-cost guard")
endif()
math(EXPR BLOCK_VIEW_LENGTH "${BLOCK_VIEW_END} - ${BLOCK_VIEW_START}")
string(SUBSTRING "${PAGE_SOURCE}" ${BLOCK_VIEW_START} ${BLOCK_VIEW_LENGTH} BLOCK_VIEW_SOURCE)
string(FIND "${BLOCK_VIEW_SOURCE}" "get_block_longhash" REPEATED_RANDOMX)
if(NOT REPEATED_RANDOMX EQUAL -1)
    message(FATAL_ERROR "Ordinary block views must not recalculate RandomX PoW")
endif()

foreach(FORBIDDEN_TEXT "/qwc-rpc" "nodeNameForPublicKey" "knownEndpointForPublicKey")
    string(FIND "${OVERVIEW_TEMPLATE}" "${FORBIDDEN_TEXT}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Overview contains forbidden adapter or identity mapping: ${FORBIDDEN_TEXT}")
    endif()
endforeach()

foreach(FORBIDDEN_FIELD "name=\"viewkey\"" "name=\"txprvkey\"")
    string(FIND "${TX_TEMPLATE}" "${FORBIDDEN_FIELD}" FOUND_AT)
    if(NOT FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Transaction page contains a secret input: ${FORBIDDEN_FIELD}")
    endif()
endforeach()
