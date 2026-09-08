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

inline std::string format_atomic_decimal_string(const std::string& atomic,
                                                unsigned decimal_places)
{
    if (atomic.empty() || atomic.find_first_not_of("0123456789") != std::string::npos)
        return "unavailable";
    std::string normalized = atomic;
    const auto first_nonzero = normalized.find_first_not_of('0');
    normalized = first_nonzero == std::string::npos ? "0" : normalized.substr(first_nonzero);
    if (decimal_places == 0)
        return normalized;
    if (normalized.size() <= decimal_places)
        normalized.insert(0, decimal_places + 1 - normalized.size(), '0');
    normalized.insert(normalized.size() - decimal_places, 1, '.');
    return normalized;
}
}

#endif
