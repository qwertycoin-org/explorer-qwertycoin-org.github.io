#ifndef QWC_EXPLORER_SUPPLY_MATH_H
#define QWC_EXPLORER_SUPPLY_MATH_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace xmreg
{
using supply_wide = boost::multiprecision::uint128_t;

struct supply_record
{
    uint64_t height {0};
    std::string hash;
    std::string parent_hash;
    uint64_t coinbase_atomic {0};
    uint64_t fees_atomic {0};
    uint64_t minted_atomic {0};
    supply_wide cumulative_coinbase {0};
    supply_wide cumulative_fees {0};
    supply_wide cumulative_minted {0};
};

inline bool checked_supply_add(const supply_wide& lhs,
                               const supply_wide& rhs,
                               supply_wide& result)
{
    const supply_wide maximum = (std::numeric_limits<supply_wide>::max)();
    if (rhs > maximum - lhs)
        return false;
    result = lhs + rhs;
    return true;
}

inline bool minted_delta(uint64_t coinbase_atomic,
                         uint64_t fees_atomic,
                         uint64_t& minted_atomic)
{
    if (fees_atomic > coinbase_atomic)
        return false;
    minted_atomic = coinbase_atomic - fees_atomic;
    return true;
}

inline bool append_supply_record(std::vector<supply_record>& records,
                                 supply_record record)
{
    if (record.height != records.size() || record.hash.empty()
        || (record.height == 0 && !record.parent_hash.empty())
        || (record.height > 0 && record.parent_hash != records.back().hash)
        || !minted_delta(record.coinbase_atomic, record.fees_atomic,
                         record.minted_atomic))
        return false;
    const supply_wide prior_coinbase = records.empty() ? 0 : records.back().cumulative_coinbase;
    const supply_wide prior_fees = records.empty() ? 0 : records.back().cumulative_fees;
    const supply_wide prior_minted = records.empty() ? 0 : records.back().cumulative_minted;
    if (!checked_supply_add(prior_coinbase, record.coinbase_atomic, record.cumulative_coinbase)
        || !checked_supply_add(prior_fees, record.fees_atomic, record.cumulative_fees)
        || !checked_supply_add(prior_minted, record.minted_atomic, record.cumulative_minted))
        return false;
    records.push_back(std::move(record));
    return true;
}

template<typename HashAtHeight>
inline void rollback_supply_records(std::vector<supply_record>& records,
                                    uint64_t canonical_block_count,
                                    HashAtHeight hash_at_height)
{
    while (!records.empty())
    {
        const auto& last = records.back();
        if (last.height < canonical_block_count
            && hash_at_height(last.height) == last.hash)
            return;
        records.pop_back();
    }
}

inline std::string supply_wide_to_string(const supply_wide& value)
{
    return value.convert_to<std::string>();
}

inline bool parse_supply_wide(const std::string& text, supply_wide& value)
{
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        return false;
    try
    {
        std::istringstream input {text};
        boost::multiprecision::cpp_int candidate {0};
        input >> candidate;
        if (!input || !input.eof() || candidate < 0
            || candidate > (std::numeric_limits<supply_wide>::max)())
            return false;
        value = candidate.convert_to<supply_wide>();
        return true;
    }
    catch (...)
    {
        return false;
    }
}
}

#endif
