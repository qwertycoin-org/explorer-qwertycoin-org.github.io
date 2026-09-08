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

struct descending_block_range
{
    uint64_t begin {0};
    uint64_t end_exclusive {0};
    uint64_t returned_count {0};
    bool empty {true};
};

inline descending_block_range make_descending_block_range(uint64_t block_count,
                                                          uint64_t offset,
                                                          uint64_t limit)
{
    if (limit == 0 || offset >= block_count)
        return {};
    const uint64_t end_exclusive = block_count - offset;
    const uint64_t begin = end_exclusive > limit ? end_exclusive - limit : 0;
    return {begin, end_exclusive, end_exclusive - begin, false};
}

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
