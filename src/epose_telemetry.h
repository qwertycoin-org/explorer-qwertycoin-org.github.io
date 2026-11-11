// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace xmreg
{

enum class epose_telemetry_state
{
    reported,
    not_reported,
    endpoint_unavailable,
    unreachable,
    rejected,
    not_checked_capacity
};

struct epose_remote_telemetry
{
    epose_telemetry_state state {epose_telemetry_state::unreachable};
    uint64_t telemetry_version {0};
    std::string software_version;
    uint64_t tip_height {0};
    std::string tip_hash;
    bool synchronized {false};
    uint64_t observed_at_unix {0};
};

struct epose_tip_assessment
{
    std::string status;
    uint64_t blocks_behind {0};
};

inline bool is_lower_hex_telemetry_hash(const std::string& value)
{
    return value.size() == 64
        && std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
}

inline bool valid_epose_software_version(const std::string& value)
{
    return !value.empty() && value.size() <= 64
        && std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c >= 0x20 && c <= 0x7e;
        });
}

inline bool valid_epose_dns_name(const std::string& host)
{
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.')
        return false;

    std::size_t label_start = 0;
    bool has_non_numeric = false;
    while (label_start < host.size())
    {
        const std::size_t label_end = host.find('.', label_start);
        const std::size_t end = label_end == std::string::npos ? host.size() : label_end;
        const std::size_t length = end - label_start;
        if (length == 0 || length > 63 || host[label_start] == '-' || host[end - 1] == '-')
            return false;
        for (std::size_t i = label_start; i < end; ++i)
        {
            const unsigned char c = static_cast<unsigned char>(host[i]);
            const bool ascii_letter = (c >= 'A' && c <= 'Z')
                || (c >= 'a' && c <= 'z');
            const bool ascii_digit = c >= '0' && c <= '9';
            if (!(ascii_letter || ascii_digit || c == '-'))
                return false;
            has_non_numeric = has_non_numeric || ascii_letter || c == '-';
        }
        if (label_end == std::string::npos)
            break;
        label_start = label_end + 1;
    }
    return has_non_numeric;
}

inline bool is_public_epose_address(const boost::asio::ip::address& address)
{
    if (address.is_unspecified() || address.is_loopback() || address.is_multicast())
        return false;

    if (address.is_v4())
    {
        const auto bytes = address.to_v4().to_bytes();
        const uint8_t a = bytes[0], b = bytes[1], c = bytes[2];
        return !(a == 0 || a == 10 || a == 127 || a >= 224
            || (a == 100 && b >= 64 && b <= 127)
            || (a == 169 && b == 254)
            || (a == 172 && b >= 16 && b <= 31)
            || (a == 192 && b == 0 && c == 0)
            || (a == 192 && b == 0 && c == 2)
            || (a == 192 && b == 88 && c == 99)
            || (a == 192 && b == 168)
            || (a == 198 && (b == 18 || b == 19))
            || (a == 198 && b == 51 && c == 100)
            || (a == 203 && b == 0 && c == 113));
    }

    const auto bytes = address.to_v6().to_bytes();
    const bool global_unicast = (bytes[0] & 0xe0) == 0x20;
    const bool documentation = bytes[0] == 0x20 && bytes[1] == 0x01
        && bytes[2] == 0x0d && bytes[3] == 0xb8;
    const bool ipv4_mapped = std::all_of(bytes.begin(), bytes.begin() + 10,
            [](uint8_t value) { return value == 0; })
        && bytes[10] == 0xff && bytes[11] == 0xff;
    return global_unicast && !documentation && !ipv4_mapped;
}

inline bool is_public_epose_address(const std::string& literal)
{
    boost::system::error_code error;
    const auto address = boost::asio::ip::address::from_string(literal, error);
    return !error && is_public_epose_address(address);
}

inline epose_tip_assessment assess_epose_tip(
        uint64_t remote_height,
        const std::string& remote_hash,
        uint64_t observer_tip_height,
        const std::string& canonical_hash_at_remote_height)
{
    if (!is_lower_hex_telemetry_hash(remote_hash))
        return {"invalid_report", 0};
    if (remote_height > observer_tip_height)
        return {"ahead_unverified", 0};
    if (!is_lower_hex_telemetry_hash(canonical_hash_at_remote_height))
        return {"observer_unavailable", 0};
    if (remote_hash != canonical_hash_at_remote_height)
        return {"hash_mismatch", 0};
    if (remote_height == observer_tip_height)
        return {"same_tip", 0};
    return {"canonical_behind", observer_tip_height - remote_height};
}

epose_remote_telemetry fetch_epose_remote_telemetry(
        const std::string& host,
        uint64_t port,
        uint64_t transport,
        const std::string& descriptor_hash,
        const std::string& service_public_key,
        uint64_t timeout_ms = 750);

}
