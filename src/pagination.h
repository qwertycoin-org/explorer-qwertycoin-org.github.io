#ifndef QWC_EXPLORER_PAGINATION_H
#define QWC_EXPLORER_PAGINATION_H

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>

namespace xmreg
{
struct bounded_pagination
{
    uint64_t page {0};
    uint64_t limit {0};
    uint64_t offset {0};
};

inline bool parse_uint64_strict(const std::string& input, uint64_t& value)
{
    if (input.empty())
        return false;
    const char* first = input.data();
    const char* last = first + input.size();
    const auto result = std::from_chars(first, last, value, 10);
    return result.ec == std::errc{} && result.ptr == last;
}

inline bool parse_bounded_pagination(const std::string& page_text,
                                     const std::string& limit_text,
                                     uint64_t maximum_limit,
                                     bounded_pagination& result)
{
    uint64_t page {0};
    uint64_t limit {0};
    if (!parse_uint64_strict(page_text, page)
        || !parse_uint64_strict(limit_text, limit)
        || limit == 0 || limit > maximum_limit
        || page > std::numeric_limits<uint64_t>::max() / limit)
        return false;

    result = {page, limit, page * limit};
    return true;
}
}

#endif
