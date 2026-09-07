#include "src/exact_amount.h"
#include "src/pagination.h"

#include <cstdint>
#include <limits>
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
    constexpr uint64_t coin = 100000000;
    bool ok = true;
    ok &= expect(xmreg::format_atomic_amount(9007199254740993ULL, coin, 8)
                 == "90071992.54740993", "large exact amount");
    ok &= expect(xmreg::format_atomic_amount(1, coin, 8) == "0.00000001", "one atomic unit");
    ok &= expect(xmreg::format_atomic_amount(0, coin, 8) == "0.00000000", "zero amount");

    xmreg::bounded_pagination page;
    ok &= expect(xmreg::parse_bounded_pagination("0", "25", 100, page), "valid first page");
    ok &= expect(page.offset == 0 && page.limit == 25, "first page values");
    ok &= expect(xmreg::parse_bounded_pagination("3", "100", 100, page), "valid bounded page");
    ok &= expect(page.offset == 300, "checked offset");
    ok &= expect(!xmreg::parse_bounded_pagination("-1", "25", 100, page), "negative page rejected");
    ok &= expect(!xmreg::parse_bounded_pagination("1x", "25", 100, page), "malformed page rejected");
    ok &= expect(!xmreg::parse_bounded_pagination("0", "0", 100, page), "zero limit rejected");
    ok &= expect(!xmreg::parse_bounded_pagination("0", "101", 100, page), "oversized limit rejected");
    ok &= expect(!xmreg::parse_bounded_pagination(std::to_string(std::numeric_limits<uint64_t>::max()), "2", 100, page), "offset overflow rejected");
    return ok ? 0 : 1;
}
