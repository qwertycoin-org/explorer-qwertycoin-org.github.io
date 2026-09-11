file(READ "${SOURCE_DIR}/main.cpp" MAIN_SOURCE)
file(READ "${SOURCE_DIR}/src/templates/index2.html" OVERVIEW_TEMPLATE)
file(READ "${SOURCE_DIR}/src/templates/css/style.css" STYLE_SOURCE)
file(READ "${SOURCE_DIR}/src/templates/partials/tx_details.html" TX_TEMPLATE)
file(READ "${SOURCE_DIR}/src/page.h" PAGE_SOURCE)
file(READ "${SOURCE_DIR}/deploy/explorer.qwertycoin.org.nginx.conf" NGINX_SOURCE)
file(READ "${SOURCE_DIR}/src/wallet_rpc_policy.h" RPC_POLICY_SOURCE)
file(READ "${SOURCE_DIR}/src/CmdLineOptions.cpp" OPTIONS_SOURCE)

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

string(FIND "${STYLE_SOURCE}" ".blocks-table .optional-block-field { display: none; }" MOBILE_OPTIONAL_FIELDS)
if(MOBILE_OPTIONAL_FIELDS EQUAL -1)
    message(FATAL_ERROR "Mobile blocks must hide only explicitly optional fields")
endif()
string(FIND "${STYLE_SOURCE}" ".blocks-table td:nth-child(7)" HIDDEN_BLOCK_HASH)
if(NOT HIDDEN_BLOCK_HASH EQUAL -1)
    message(FATAL_ERROR "Mobile block hash must not be hidden by column position")
endif()

foreach(REQUIRED_REFRESH_TEXT
        "setTimeout(refresh, ms)"
        "new AbortController()"
        "document.hidden"
        "Math.min(delay * 2, 120000)"
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
        "proxy_pass http://qwertycoin_wallet_rpc_backend")
    string(FIND "${NGINX_SOURCE}" "${REQUIRED_RPC_EDGE_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Restricted wallet RPC edge contract is missing: ${REQUIRED_RPC_EDGE_TEXT}")
    endif()
endforeach()

foreach(REQUIRED_RPC_POLICY_TEXT
        "getblocks.bin"
        "get_outs.bin"
        "send_raw_transaction"
        "get_info"
        "get_output_histogram"
        "wallet_rpc_policy_result::forbidden")
    string(FIND "${RPC_POLICY_SOURCE}" "${REQUIRED_RPC_POLICY_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Parsed wallet RPC policy contract is missing: ${REQUIRED_RPC_POLICY_TEXT}")
    endif()
endforeach()

string(FIND "${NGINX_SOURCE}" "location ^~ /qwc-rpc/" GENERIC_RPC_BLOCK)
if(NOT GENERIC_RPC_BLOCK EQUAL -1)
    message(FATAL_ERROR "The wallet RPC path was replaced by a generic prefix handler")
endif()

foreach(REQUIRED_TEXT
        "{\"qualified_count\", info.qualified_count}"
        "{\"qualified_for_source_epoch\", node.qualified}"
        "confidential_amounts ? \"confidential\" : \"public\""
        "{\"rpc_db_anchor_matches\", anchor_matches}"
        "MAINNET_FINAL_GENESIS_HASH_V2"
        "MAINNET_FINAL_PARAMETER_SET_HASH_V2")
    string(FIND "${PAGE_SOURCE}" "${REQUIRED_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Required fail-closed correctness contract is missing: ${REQUIRED_TEXT}")
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
