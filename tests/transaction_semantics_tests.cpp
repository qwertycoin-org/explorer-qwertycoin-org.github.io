#include "src/transaction_amounts.h"

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
    bool ok = true;
    cryptonote::transaction confidential;
    confidential.vin.push_back(cryptonote::txin_to_key {});
    confidential.rct_signatures.type = rct::RCTTypeCLSAG;
    ok &= expect(xmreg::transaction_amounts_confidential(confidential),
                 "real non-coinbase RingCT transaction is confidential");

    cryptonote::transaction public_regular;
    public_regular.vin.push_back(cryptonote::txin_to_key {});
    public_regular.rct_signatures.type = rct::RCTTypeNull;
    ok &= expect(!xmreg::transaction_amounts_confidential(public_regular),
                 "non-RingCT regular transaction remains public");

    cryptonote::transaction coinbase;
    coinbase.vin.push_back(cryptonote::txin_gen {});
    coinbase.rct_signatures.type = rct::RCTTypeNull;
    ok &= expect(!xmreg::transaction_amounts_confidential(coinbase),
                 "coinbase output amounts remain public");
    return ok ? 0 : 1;
}
