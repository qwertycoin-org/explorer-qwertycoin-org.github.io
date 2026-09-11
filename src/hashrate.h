#ifndef QWC_EXPLORER_HASHRATE_H
#define QWC_EXPLORER_HASHRATE_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <string>

namespace xmreg
{
using hashrate_wide = boost::multiprecision::uint128_t;

inline std::string decimal_ratio(const hashrate_wide& numerator,
                                 const hashrate_wide& denominator,
                                 unsigned decimals)
{
    if (denominator == 0)
        return "unavailable";
    boost::multiprecision::uint256_t scale {1};
    for (unsigned i = 0; i < decimals; ++i)
        scale *= 10;
    const boost::multiprecision::uint256_t scaled
            = (boost::multiprecision::uint256_t {numerator} * scale)
              / boost::multiprecision::uint256_t {denominator};
    std::string digits = scaled.convert_to<std::string>();
    if (decimals == 0)
        return digits;
    if (digits.size() <= decimals)
        digits.insert(0, decimals + 1 - digits.size(), '0');
    digits.insert(digits.size() - decimals, 1, '.');
    return digits;
}

inline std::string format_hashrate_si(const hashrate_wide& difficulty,
                                      uint64_t target)
{
    if (target == 0)
        return "unavailable";
    static const char* units[] = {"H/s", "kH/s", "MH/s", "GH/s", "TH/s", "PH/s", "EH/s"};
    hashrate_wide divisor {target};
    std::size_t unit = 0;
    while (unit + 1 < sizeof(units) / sizeof(units[0])
           && difficulty >= divisor * 1000)
    {
        divisor *= 1000;
        ++unit;
    }
    return decimal_ratio(difficulty, divisor, 3)
            + " " + units[unit];
}
}

#endif
