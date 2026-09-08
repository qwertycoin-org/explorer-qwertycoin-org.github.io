#include "SupplyIndex.h"

#include "tools.h"

#include <boost/filesystem.hpp>

#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <unistd.h>

namespace xmreg
{
namespace
{
constexpr const char* state_magic = "QWC_SUPPLY_INDEX_V2";

std::vector<std::string> split_tabs(const std::string& line)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = line.find('\t', start);
        parts.push_back(line.substr(start, end == std::string::npos
                                          ? std::string::npos : end - start));
        if (end == std::string::npos)
            return parts;
        start = end + 1;
    }
}

bool parse_u64(const std::string& text, uint64_t& value)
{
    if (text.empty())
        return false;
    std::size_t parsed = 0;
    try
    {
        const auto candidate = std::stoull(text, &parsed, 10);
        if (parsed != text.size())
            return false;
        value = candidate;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::string network_name(cryptonote::network_type nettype)
{
    if (nettype == cryptonote::network_type::TESTNET)
        return "testnet";
    if (nettype == cryptonote::network_type::STAGENET)
        return "stagenet";
    return "mainnet";
}
}

SupplyIndex::SupplyIndex(MicroCore* mcore,
                         Blockchain* core_storage,
                         std::string data_path,
                         std::string chain_reset_id,
                         cryptonote::network_type nettype,
                         uint64_t batch_size)
    : mcore_ {mcore},
      core_storage_ {core_storage},
      data_path_ {std::move(data_path)},
      state_file_ {data_path_ + "/supply-index-v2.tsv"},
      chain_reset_id_ {std::move(chain_reset_id)},
      network_name_ {network_name(nettype)},
      batch_size_ {std::max<uint64_t>(1, std::min<uint64_t>(batch_size, 4096))}
{
    const uint64_t count = core_storage_->get_current_blockchain_height();
    if (count > 0)
        genesis_hash_ = epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(0));
    snapshot_.genesis_hash = genesis_hash_;
    snapshot_.chain_reset_id = chain_reset_id_;
}

SupplyIndex::~SupplyIndex()
{
    stop();
}

bool SupplyIndex::start()
{
    if (running_.exchange(true))
        return true;
    if (data_path_.empty() || chain_reset_id_.empty() || genesis_hash_.empty())
    {
        running_ = false;
        publish("error", "Supply index configuration or genesis is unavailable", 0, "", {}, false);
        return false;
    }
    boost::system::error_code ec;
    boost::filesystem::create_directories(data_path_, ec);
    if (ec)
    {
        running_ = false;
        publish("error", "Cannot create explorer-derived data directory", 0, "", {}, false);
        return false;
    }
    load();
    stop_requested_ = false;
    thread_ = std::thread(&SupplyIndex::run, this);
    return true;
}

void SupplyIndex::stop()
{
    if (!running_.exchange(false))
        return;
    stop_requested_ = true;
    wake_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

supply_snapshot SupplyIndex::snapshot() const
{
    std::lock_guard<std::mutex> lock {mutex_};
    return snapshot_;
}

bool SupplyIndex::load()
{
    std::ifstream input {state_file_};
    if (!input)
        return true;

    std::string line;
    if (!std::getline(input, line))
        return false;
    const auto header = split_tabs(line);
    if (header.size() != 5 || header[0] != state_magic
        || header[1] != network_name_ || header[2] != genesis_hash_
        || header[3] != chain_reset_id_
        || header[4] != snapshot_.calculation_version)
        return false;

    std::vector<supply_record> loaded;
    while (std::getline(input, line))
    {
        const auto fields = split_tabs(line);
        if (fields.size() != 9)
            return false;
        supply_record record;
        if (!parse_u64(fields[0], record.height)
            || !parse_u64(fields[3], record.coinbase_atomic)
            || !parse_u64(fields[4], record.fees_atomic)
            || !parse_u64(fields[5], record.minted_atomic)
            || !parse_supply_wide(fields[6], record.cumulative_coinbase)
            || !parse_supply_wide(fields[7], record.cumulative_fees)
            || !parse_supply_wide(fields[8], record.cumulative_minted))
            return false;
        record.hash = fields[1];
        record.parent_hash = fields[2];
        if (record.height != loaded.size()
            || record.coinbase_atomic < record.fees_atomic
            || record.minted_atomic != record.coinbase_atomic - record.fees_atomic
            || (record.height == 0 && !record.parent_hash.empty())
            || (record.height > 0 && record.parent_hash != loaded.back().hash))
            return false;
        const supply_wide prior_coinbase = loaded.empty() ? 0 : loaded.back().cumulative_coinbase;
        const supply_wide prior_fees = loaded.empty() ? 0 : loaded.back().cumulative_fees;
        const supply_wide prior_minted = loaded.empty() ? 0 : loaded.back().cumulative_minted;
        supply_wide expected_coinbase, expected_fees, expected_minted;
        if (!checked_supply_add(prior_coinbase, record.coinbase_atomic, expected_coinbase)
            || !checked_supply_add(prior_fees, record.fees_atomic, expected_fees)
            || !checked_supply_add(prior_minted, record.minted_atomic, expected_minted)
            || expected_coinbase != record.cumulative_coinbase
            || expected_fees != record.cumulative_fees
            || expected_minted != record.cumulative_minted)
            return false;
        loaded.push_back(std::move(record));
    }
    records_ = std::move(loaded);
    return true;
}

bool SupplyIndex::persist(const std::vector<supply_record>& records, std::string& error) const
{
    const std::string temporary = state_file_ + ".tmp";
    {
        std::ofstream output {temporary, std::ios::trunc};
        if (!output)
        {
            error = "Cannot open temporary supply checkpoint";
            return false;
        }
        output << state_magic << '\t' << network_name_ << '\t' << genesis_hash_ << '\t'
               << chain_reset_id_ << '\t' << "actual-coinbase-minus-fees-v1" << '\n';
        for (const auto& record : records)
        {
            output << record.height << '\t' << record.hash << '\t' << record.parent_hash << '\t'
                   << record.coinbase_atomic << '\t' << record.fees_atomic << '\t'
                   << record.minted_atomic << '\t'
                   << supply_wide_to_string(record.cumulative_coinbase) << '\t'
                   << supply_wide_to_string(record.cumulative_fees) << '\t'
                   << supply_wide_to_string(record.cumulative_minted) << '\n';
        }
        output.flush();
        if (!output)
        {
            error = "Cannot flush supply checkpoint";
            return false;
        }
    }
    const int descriptor = ::open(temporary.c_str(), O_RDONLY);
    if (descriptor < 0 || ::fsync(descriptor) != 0)
    {
        if (descriptor >= 0)
            ::close(descriptor);
        error = "Cannot sync supply checkpoint";
        return false;
    }
    ::close(descriptor);
    if (::rename(temporary.c_str(), state_file_.c_str()) != 0)
    {
        error = "Cannot atomically publish supply checkpoint";
        return false;
    }
    const int directory = ::open(data_path_.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory >= 0)
    {
        ::fsync(directory);
        ::close(directory);
    }
    return true;
}

bool SupplyIndex::read_record(uint64_t height,
                              const std::string& expected_parent,
                              supply_record& record,
                              std::string& error) const
{
    block blk;
    if (!mcore_->get_block_by_height(height, blk))
    {
        error = "Cannot read canonical block " + std::to_string(height);
        return false;
    }
    const std::string hash = epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(height));
    const std::string block_hash = epee::string_tools::pod_to_hex(get_block_hash(blk));
    if (hash.empty() || block_hash != hash
        || (height > 0 && expected_parent.empty()))
    {
        error = "Cannot anchor canonical block " + std::to_string(height);
        return false;
    }
    if (height > 0)
    {
        const std::string actual_parent = epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(height - 1));
        if (actual_parent != expected_parent
            || epee::string_tools::pod_to_hex(blk.prev_id) != expected_parent)
        {
            error = "Canonical parent changed while indexing";
            return false;
        }
    }
    const uint64_t coinbase = get_outs_money_amount(blk.miner_tx);
    std::vector<transaction> transactions;
    std::vector<crypto::hash> missed;
    if (!core_storage_->get_transactions(blk.tx_hashes, transactions, missed, true)
        || !missed.empty() || transactions.size() != blk.tx_hashes.size())
    {
        error = "Cannot read every transaction for block " + std::to_string(height);
        return false;
    }
    uint64_t fees = 0;
    for (const auto& tx : transactions)
    {
        const uint64_t fee = get_tx_fee(tx);
        if (fee > std::numeric_limits<uint64_t>::max() - fees)
        {
            error = "Fee total overflow at block " + std::to_string(height);
            return false;
        }
        fees += fee;
    }
    if (fees > coinbase)
    {
        error = "Coinbase is smaller than fees at block " + std::to_string(height);
        return false;
    }
    record.height = height;
    record.hash = hash;
    record.parent_hash = height == 0 ? "" : expected_parent;
    record.coinbase_atomic = coinbase;
    record.fees_atomic = fees;
    if (!minted_delta(coinbase, fees, record.minted_atomic))
    {
        error = "Invalid issuance delta at block " + std::to_string(height);
        return false;
    }
    return true;
}

bool SupplyIndex::update_once(std::string& error)
{
    const uint64_t block_count = core_storage_->get_current_blockchain_height();
    if (block_count == 0)
    {
        error = "Canonical chain is empty";
        return false;
    }
    if (epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(0)) != genesis_hash_)
    {
        error = "Genesis changed; supply index reset context is invalid";
        return false;
    }
    const std::string observed_tip = epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(block_count - 1));

    std::vector<supply_record> candidate;
    {
        std::lock_guard<std::mutex> lock {mutex_};
        candidate = records_;
    }
    rollback_supply_records(candidate, block_count, [this](uint64_t height) {
        return epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(height));
    });

    const uint64_t target = std::min<uint64_t>(block_count,
                                               candidate.size() + batch_size_);
    while (candidate.size() < target)
    {
        supply_record record;
        const std::string parent = candidate.empty() ? "" : candidate.back().hash;
        if (!read_record(candidate.size(), parent, record, error))
            return false;
        if (!append_supply_record(candidate, std::move(record)))
        {
            error = "Invalid or overflowing cumulative supply record";
            return false;
        }
    }
    if (!candidate.empty()
        && epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(candidate.back().height)) != candidate.back().hash)
    {
        error = "Canonical chain changed during supply batch";
        return false;
    }
    if (!persist(candidate, error))
        return false;

    const uint64_t final_count = core_storage_->get_current_blockchain_height();
    const std::string final_tip = final_count > 0
            ? epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(final_count - 1)) : "";
    const bool complete = final_count == candidate.size()
            && final_count == block_count && final_tip == observed_tip;
    {
        std::lock_guard<std::mutex> lock {mutex_};
        records_ = candidate;
    }
    publish(complete ? "current" : "indexing", "", final_count, final_tip,
            candidate, complete);
    return true;
}

void SupplyIndex::publish(const std::string& availability,
                          const std::string& message,
                          uint64_t block_count,
                          const std::string& tip_hash,
                          const std::vector<supply_record>& records,
                          bool complete)
{
    supply_snapshot next;
    next.availability = availability;
    next.message = message;
    next.genesis_hash = genesis_hash_;
    next.chain_reset_id = chain_reset_id_;
    next.calculation_version = "actual-coinbase-minus-fees-v1";
    next.block_count = block_count;
    next.tip_height = block_count > 0 ? block_count - 1 : 0;
    next.tip_hash = tip_hash;
    next.indexed_block_count = records.size();
    next.observed_at = static_cast<uint64_t>(std::time(nullptr));
    next.complete = complete;
    if (!records.empty())
    {
        const auto& last = records.back();
        next.indexed_through_height = last.height;
        next.indexed_through_hash = last.hash;
        next.minted_supply_atomic = supply_wide_to_string(last.cumulative_minted);
        next.gross_coinbase_atomic = supply_wide_to_string(last.cumulative_coinbase);
        next.cumulative_fees_atomic = supply_wide_to_string(last.cumulative_fees);
    }
    std::lock_guard<std::mutex> lock {mutex_};
    snapshot_ = std::move(next);
}

void SupplyIndex::run()
{
    while (!stop_requested_)
    {
        std::string error;
        if (!update_once(error))
        {
            const uint64_t count = core_storage_->get_current_blockchain_height();
            const std::string tip = count > 0
                    ? epee::string_tools::pod_to_hex(core_storage_->get_block_id_by_height(count - 1)) : "";
            std::vector<supply_record> records;
            {
                std::lock_guard<std::mutex> lock {mutex_};
                records = records_;
            }
            publish("error", error, count, tip, records, false);
        }
        std::unique_lock<std::mutex> lock {mutex_};
        const auto delay = snapshot_.complete ? std::chrono::seconds(15)
                                              : std::chrono::milliseconds(100);
        wake_.wait_for(lock, delay, [this] { return stop_requested_.load(); });
    }
}

}
