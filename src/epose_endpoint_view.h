// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <string>

namespace xmreg
{

inline const char* epose_endpoint_transport_name(uint64_t transport)
{
    switch (transport)
    {
        case 1: return "tcp_ipv4";
        case 2: return "tcp_ipv6";
        case 3: return "dns";
        default: return "unknown";
    }
}

inline bool valid_epose_advertised_endpoint(
        bool ready,
        const std::string& descriptor_hash,
        const std::string& expected_descriptor_hash,
        const std::string& service_public_key,
        const std::string& expected_service_public_key,
        const std::string& host,
        uint64_t port,
        uint64_t transport)
{
    return ready
        && !expected_descriptor_hash.empty()
        && descriptor_hash == expected_descriptor_hash
        && !expected_service_public_key.empty()
        && service_public_key == expected_service_public_key
        && !host.empty()
        && port > 0 && port <= 65535
        && transport >= 1 && transport <= 3;
}

inline std::string format_epose_endpoint_authority(
        const std::string& host, uint64_t port, uint64_t transport)
{
    if (host.empty() || port == 0 || port > 65535)
        return {};
    if (transport == 2)
        return "[" + host + "]:" + std::to_string(port);
    return host + ":" + std::to_string(port);
}

}
