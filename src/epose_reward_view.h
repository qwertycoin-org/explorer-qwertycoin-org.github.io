#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace xmreg
{

struct epose_reward_output_view
{
    uint64_t index {0};
    uint64_t amount {0};
    std::string public_key;
};

struct epose_reward_view
{
    uint64_t payout_epoch {0};
    uint64_t source_epoch {0};
    uint64_t qualified_count {0};
    uint64_t scheduled_subsidy {0};
    uint64_t transaction_fees {0};
    uint64_t miner_subsidy {0};
    uint64_t miner_fees {0};
    uint64_t issued_subsidy {0};
    uint64_t emission_advance {0};
    uint64_t coinbase_total {0};
    uint64_t miner_reward {0};
    uint64_t service_reward {0};
    uint64_t permanently_unissued {0};
    bool service_reward_active {false};
    std::string qualification_hash;
    std::string payee_service_public_key;
    std::vector<epose_reward_output_view> service_outputs;
};

inline bool is_lower_hex_64(const std::string& value)
{
    if (value.size() != 64)
        return false;
    for (const char c : value)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}

template<typename Response>
bool make_epose_reward_view(
        const Response& response,
        const std::string& expected_block_hash,
        uint64_t expected_height,
        uint64_t expected_coinbase_total,
        size_t expected_coinbase_output_count,
        epose_reward_view& view)
{
    view = {};
    if (!response.mapping_available
        || response.block_hash != expected_block_hash
        || response.height != expected_height
        || response.coinbase_total != expected_coinbase_total
        || !is_lower_hex_64(response.block_hash)
        || !is_lower_hex_64(response.parent_hash)
        || !is_lower_hex_64(response.qualification_hash))
        return false;

    uint64_t issued_from_components {0};
    uint64_t scheduled_from_components {0};
    if (response.miner_subsidy
            > std::numeric_limits<uint64_t>::max() - response.service_reward)
        return false;
    issued_from_components = response.miner_subsidy + response.service_reward;
    if (issued_from_components
            > std::numeric_limits<uint64_t>::max() - response.permanently_unissued)
        return false;
    scheduled_from_components =
            issued_from_components + response.permanently_unissued;

    if (response.miner_reward
        > std::numeric_limits<uint64_t>::max() - response.service_reward
        || response.miner_reward + response.service_reward
            != response.coinbase_total
        || response.issued_subsidy
            > std::numeric_limits<uint64_t>::max() - response.transaction_fees
        || response.issued_subsidy + response.transaction_fees
            != response.coinbase_total
        || response.miner_subsidy
            > std::numeric_limits<uint64_t>::max() - response.miner_fees
        || response.miner_subsidy + response.miner_fees
            != response.miner_reward
        || response.miner_fees != response.transaction_fees
        || response.issued_subsidy != issued_from_components
        || response.scheduled_subsidy != scheduled_from_components
        || response.emission_advance != response.scheduled_subsidy)
        return false;

    uint64_t output_total {0};
    std::set<uint64_t> output_indices;
    std::vector<epose_reward_output_view> outputs;
    outputs.reserve(response.service_outputs.size());
    for (const auto& output : response.service_outputs)
    {
        if (output.index >= expected_coinbase_output_count
            || !output_indices.insert(output.index).second
            || !is_lower_hex_64(output.public_key)
            || output_total > std::numeric_limits<uint64_t>::max() - output.amount)
            return false;
        output_total += output.amount;
        outputs.push_back({output.index, output.amount, output.public_key});
    }

    if (response.service_reward_active)
    {
        if (!response.payment_proof_valid || response.service_reward == 0
            || response.qualified_count == 0 || outputs.empty()
            || output_total != response.service_reward
            || !is_lower_hex_64(response.payee_service_public_key)
            || !is_lower_hex_64(response.reward_view_public_key)
            || !is_lower_hex_64(response.reward_spend_public_key))
            return false;
    }
    else if (response.payment_proof_valid || response.service_reward != 0
             || !outputs.empty())
    {
        return false;
    }

    view.payout_epoch = response.payout_epoch;
    view.source_epoch = response.source_epoch;
    view.qualified_count = response.qualified_count;
    view.scheduled_subsidy = response.scheduled_subsidy;
    view.transaction_fees = response.transaction_fees;
    view.miner_subsidy = response.miner_subsidy;
    view.miner_fees = response.miner_fees;
    view.issued_subsidy = response.issued_subsidy;
    view.emission_advance = response.emission_advance;
    view.coinbase_total = response.coinbase_total;
    view.miner_reward = response.miner_reward;
    view.service_reward = response.service_reward;
    view.permanently_unissued = response.permanently_unissued;
    view.service_reward_active = response.service_reward_active;
    view.qualification_hash = response.qualification_hash;
    view.payee_service_public_key = response.payee_service_public_key;
    view.service_outputs = std::move(outputs);
    return true;
}

}
