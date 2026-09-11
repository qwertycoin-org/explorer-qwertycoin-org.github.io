#ifndef QWC_EXPLORER_SUPPLY_INDEX_H
#define QWC_EXPLORER_SUPPLY_INDEX_H

#include "MicroCore.h"
#include "supply_math.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace xmreg
{

struct supply_snapshot
{
    std::string availability {"indexing"};
    std::string message;
    std::string genesis_hash;
    std::string chain_reset_id;
    std::string calculation_version {"actual-coinbase-minus-fees-v1"};
    uint64_t block_count {0};
    uint64_t tip_height {0};
    std::string tip_hash;
    uint64_t indexed_block_count {0};
    uint64_t indexed_through_height {0};
    std::string indexed_through_hash;
    std::string minted_supply_atomic {"0"};
    std::string gross_coinbase_atomic {"0"};
    std::string cumulative_fees_atomic {"0"};
    uint64_t observed_at {0};
    bool complete {false};
};

class SupplyIndex
{
public:
    SupplyIndex(MicroCore* mcore,
                Blockchain* core_storage,
                std::string data_path,
                std::string chain_reset_id,
                cryptonote::network_type nettype,
                uint64_t batch_size = 128);
    ~SupplyIndex();

    bool start();
    void stop();
    supply_snapshot snapshot() const;

private:
    bool load();
    bool persist(const std::vector<supply_record>& records, std::string& error) const;
    bool update_once(std::string& error);
    bool read_record(uint64_t height,
                     const std::string& expected_parent,
                     supply_record& record,
                     std::string& error) const;
    void publish(const std::string& availability,
                 const std::string& message,
                 uint64_t block_count,
                 const std::string& tip_hash,
                 const std::vector<supply_record>& records,
                 bool complete);
    void run();

    MicroCore* mcore_;
    Blockchain* core_storage_;
    std::string data_path_;
    std::string state_file_;
    std::string chain_reset_id_;
    std::string network_name_;
    std::string genesis_hash_;
    uint64_t batch_size_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<supply_record> records_;
    supply_snapshot snapshot_;
    std::thread thread_;
    std::atomic<bool> stop_requested_ {false};
    std::atomic<bool> running_ {false};
};

}

#endif
