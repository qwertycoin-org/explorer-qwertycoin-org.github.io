#ifndef QWC_EXPLORER_TRANSACTION_AMOUNTS_H
#define QWC_EXPLORER_TRANSACTION_AMOUNTS_H

#include "monero_headers.h"

namespace xmreg
{
inline bool transaction_amounts_confidential(const cryptonote::transaction& tx)
{
    return !cryptonote::is_coinbase(tx)
            && tx.rct_signatures.type != rct::RCTTypeNull;
}
}

#endif
