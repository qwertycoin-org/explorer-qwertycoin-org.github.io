#ifndef QWERTYCOIN_EXPLORER_EPOSE_SNAPSHOT_H
#define QWERTYCOIN_EXPLORER_EPOSE_SNAPSHOT_H

#include <cstdint>
#include <string>

namespace xmreg
{

struct epose_chain_anchor
{
    uint64_t block_count {0};
    std::string tip_hash;
};

inline bool
valid_epose_chain_anchor(const epose_chain_anchor& anchor)
{
    return anchor.block_count > 0 && anchor.tip_hash.size() == 64;
}

inline bool
same_epose_chain_anchor(const epose_chain_anchor& lhs,
                        const epose_chain_anchor& rhs)
{
    return valid_epose_chain_anchor(lhs)
        && valid_epose_chain_anchor(rhs)
        && lhs.block_count == rhs.block_count
        && lhs.tip_hash == rhs.tip_hash;
}

inline bool
epose_reward_matches_anchor(const epose_chain_anchor& anchor,
                            uint64_t reward_height)
{
    return valid_epose_chain_anchor(anchor)
        && reward_height == anchor.block_count;
}

} // namespace xmreg

#endif // QWERTYCOIN_EXPLORER_EPOSE_SNAPSHOT_H
