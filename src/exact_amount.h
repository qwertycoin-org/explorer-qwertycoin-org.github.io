#ifndef QWC_EXPLORER_EXACT_AMOUNT_H
#define QWC_EXPLORER_EXACT_AMOUNT_H

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace xmreg
{
inline std::string format_atomic_amount(uint64_t amount, uint64_t coin, unsigned decimal_places)
{
    if (coin == 0)
        return "unavailable";

    std::ostringstream out;
    out << (amount / coin);
    if (decimal_places > 0)
        out << '.' << std::setw(decimal_places) << std::setfill('0') << (amount % coin);
    return out.str();
}
}

#endif
