#include "src/exact_amount.h"
#include "src/pagination.h"
#include "src/hashrate.h"
#include "src/supply_math.h"
#include "src/wallet_rpc_policy.h"

#include <cstdint>
#include <limits>
#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}
}

int main()
{
    constexpr uint64_t coin = 100000000;
    bool ok = true;
    ok &= expect(xmreg::format_atomic_amount(9007199254740993ULL, coin, 8)
                 == "90071992.54740993", "large exact amount");
    ok &= expect(xmreg::format_atomic_amount(1, coin, 8) == "0.00000001", "one atomic unit");
    ok &= expect(xmreg::format_atomic_amount(0, coin, 8) == "0.00000000", "zero amount");

    xmreg::bounded_pagination page;
    ok &= expect(xmreg::parse_bounded_pagination("0", "25", 100, page), "valid first page");
    ok &= expect(page.offset == 0 && page.limit == 25, "first page values");
    ok &= expect(xmreg::parse_bounded_pagination("3", "100", 100, page), "valid bounded page");
    ok &= expect(page.offset == 300, "checked offset");
    ok &= expect(!xmreg::parse_bounded_pagination("-1", "25", 100, page), "negative page rejected");
    ok &= expect(!xmreg::parse_bounded_pagination("1x", "25", 100, page), "malformed page rejected");
    ok &= expect(!xmreg::parse_bounded_pagination("0", "0", 100, page), "zero limit rejected");
    ok &= expect(!xmreg::parse_bounded_pagination("0", "101", 100, page), "oversized limit rejected");
    ok &= expect(!xmreg::parse_bounded_pagination(std::to_string(std::numeric_limits<uint64_t>::max()), "2", 100, page), "offset overflow rejected");

    const auto page9 = xmreg::make_descending_block_range(262, 9 * 25, 25);
    const auto page10 = xmreg::make_descending_block_range(262, 10 * 25, 25);
    ok &= expect(!page9.empty && page9.begin == 12 && page9.end_exclusive == 37
                 && page9.returned_count == 25, "262/25 penultimate range");
    ok &= expect(!page10.empty && page10.begin == 0 && page10.end_exclusive == 12
                 && page10.returned_count == 12, "262/25 final range without overlap");
    ok &= expect(xmreg::make_descending_block_range(262, 275, 25).empty,
                 "out of range block page is empty");

    xmreg::hashrate_wide difficulty {335310};
    ok &= expect(xmreg::decimal_ratio(difficulty, xmreg::hashrate_wide {120}, 2)
                 == "2794.25", "exact difficulty target hashrate");
    ok &= expect(xmreg::format_hashrate_si(difficulty, 120) == "2.794 kH/s",
                 "hashrate SI format");
    ok &= expect(xmreg::format_hashrate_si(difficulty, 0) == "unavailable",
                 "zero target rejected");

    uint64_t minted {0};
    ok &= expect(xmreg::minted_delta(1025000000, 25000000, minted)
                 && minted == 1000000000, "fees excluded from actual issuance");
    ok &= expect(xmreg::minted_delta(0, 0, minted) && minted == 0,
                 "zero issuance accepted");
    ok &= expect(!xmreg::minted_delta(1, 2, minted), "invalid issuance rejected");
    xmreg::supply_wide wide_total {std::numeric_limits<uint64_t>::max()};
    xmreg::supply_wide wide_result;
    ok &= expect(xmreg::checked_supply_add(wide_total, wide_total, wide_result)
                 && wide_result > std::numeric_limits<uint64_t>::max(),
                 "wide cumulative issuance");
    ok &= expect(xmreg::format_atomic_decimal_string("9007199254740993", 8)
                 == "90071992.54740993", "wide decimal-string amount");
    xmreg::supply_wide parsed_supply;
    ok &= expect(xmreg::parse_supply_wide(
            "340282366920938463463374607431768211455", parsed_supply)
            && parsed_supply == (std::numeric_limits<xmreg::supply_wide>::max)(),
            "maximum uint128 checkpoint accepted");
    ok &= expect(!xmreg::parse_supply_wide(
            "340282366920938463463374607431768211456", parsed_supply),
            "overflowing uint128 checkpoint rejected");
    std::vector<xmreg::supply_record> supply_records;
    ok &= expect(xmreg::append_supply_record(supply_records,
            {0, "genesis", "", 1025000000, 25000000}),
            "genesis supply record");
    ok &= expect(xmreg::append_supply_record(supply_records,
            {1, "block-one", "genesis", 2000000000, 0}),
            "multiple canonical supply records");
    ok &= expect(supply_records.back().cumulative_minted == xmreg::supply_wide {3000000000ULL},
            "locked rewards included in cumulative issuance");
    supply_records.resize(1);
    ok &= expect(xmreg::append_supply_record(supply_records,
            {1, "replacement", "genesis", 1500000000, 0})
            && supply_records.back().hash == "replacement"
            && supply_records.back().cumulative_minted == xmreg::supply_wide {2500000000ULL},
            "reorg rollback and replacement delta");
    ok &= expect(!xmreg::append_supply_record(supply_records,
            {2, "bad-parent", "orphan", 1, 0}), "wrong parent rejected");
    xmreg::rollback_supply_records(supply_records, 2, [](uint64_t height) {
        return height == 0 ? std::string("genesis") : std::string("other-same-height-tip");
    });
    ok &= expect(supply_records.size() == 1 && supply_records.back().hash == "genesis",
            "same-height reorg rolls back to the common ancestor");
    xmreg::rollback_supply_records(supply_records, 0, [](uint64_t) {
        return std::string("genesis");
    });
    ok &= expect(supply_records.empty(), "chain reset invalidates prior records");

    std::string method;
    ok &= expect(xmreg::authorize_wallet_json_rpc(
            R"({"jsonrpc":"2.0","id":1,"method":"get_info","params":{}})", method)
            == xmreg::wallet_rpc_policy_result::allowed && method == "get_info",
            "wallet JSON-RPC read method allowed");
    ok &= expect(xmreg::authorize_wallet_json_rpc(
            R"({"jsonrpc":"2.0","id":1,"method":"submit_block","params":[]})", method)
            == xmreg::wallet_rpc_policy_result::forbidden,
            "wallet JSON-RPC mining method rejected");
    ok &= expect(xmreg::authorize_wallet_json_rpc(
            R"([{"jsonrpc":"2.0","id":1,"method":"get_info"}])", method)
            == xmreg::wallet_rpc_policy_result::malformed,
            "wallet JSON-RPC batch rejected");
    ok &= expect(xmreg::authorize_wallet_json_rpc(
            R"({"jsonrpc":"2.0","method":"get_info"})", method)
            == xmreg::wallet_rpc_policy_result::malformed,
            "wallet JSON-RPC notification rejected");
    ok &= expect(xmreg::wallet_rpc_path_allowed("/getblocks.bin")
                 && !xmreg::wallet_rpc_path_allowed("/getblocktemplate"),
                 "wallet RPC path boundary");
    return ok ? 0 : 1;
}
