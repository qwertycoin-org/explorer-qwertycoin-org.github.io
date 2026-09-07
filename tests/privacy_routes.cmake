file(READ "${SOURCE_DIR}/main.cpp" MAIN_SOURCE)
file(READ "${SOURCE_DIR}/src/templates/index2.html" OVERVIEW_TEMPLATE)
file(READ "${SOURCE_DIR}/src/templates/partials/tx_details.html" TX_TEMPLATE)
file(READ "${SOURCE_DIR}/src/page.h" PAGE_SOURCE)

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

foreach(REQUIRED_TEXT
        "{\"qualified_count\", info.qualified_count}"
        "{\"qualified_for_source_epoch\", node.qualified}"
        "confidential_amounts ? \"confidential\" : \"public\""
        "{\"rpc_db_anchor_matches\", anchor_matches}")
    string(FIND "${PAGE_SOURCE}" "${REQUIRED_TEXT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Required fail-closed correctness contract is missing: ${REQUIRED_TEXT}")
    endif()
endforeach()

string(FIND "${PAGE_SOURCE}" "current_network_info.current = true" CURRENT_OVERRIDE)
if(NOT CURRENT_OVERRIDE EQUAL -1)
    message(FATAL_ERROR "A cached RPC failure can be overwritten as current")
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
