#ifndef QWC_EXPLORER_WALLET_RPC_POLICY_H
#define QWC_EXPLORER_WALLET_RPC_POLICY_H

#include "../ext/json.hpp"

#include <set>
#include <string>

namespace xmreg
{
enum class wallet_rpc_policy_result { allowed, malformed, forbidden };

inline bool wallet_rpc_path_allowed(const std::string& path)
{
    static const std::set<std::string> paths {
        "/json_rpc", "/getblocks.bin", "/getblocks_by_height.bin",
        "/gethashes.bin", "/get_o_indexes.bin", "/get_output_distribution.bin",
        "/get_outs", "/get_outs.bin", "/get_transactions", "/gettransactions",
        "/get_transaction_pool_hashes.bin", "/is_key_image_spent",
        "/send_raw_transaction", "/submit_raw_tx", "/sendrawtransaction"
    };
    return paths.count(path) == 1;
}

inline wallet_rpc_policy_result authorize_wallet_json_rpc(const std::string& body,
                                                          std::string& method)
{
    static const std::set<std::string> methods {
        "get_info", "get_version", "get_block_count", "get_fee_estimate",
        "get_last_block_header", "get_block_header_by_height", "getblockheaderbyheight",
        "get_block_headers_range", "getblockheadersrange", "hard_fork_info",
        "get_output_histogram", "get_transactions"
    };
    try
    {
        const auto payload = nlohmann::json::parse(body);
        if (!payload.is_object() || payload.value("jsonrpc", std::string {}) != "2.0"
            || !payload.contains("id") || !payload.at("method").is_string())
            return wallet_rpc_policy_result::malformed;
        method = payload.at("method").get<std::string>();
        return methods.count(method) == 1 ? wallet_rpc_policy_result::allowed
                                          : wallet_rpc_policy_result::forbidden;
    }
    catch (...)
    {
        return wallet_rpc_policy_result::malformed;
    }
}
}

#endif
