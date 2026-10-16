// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose_telemetry.h"
#include "monero_headers.h"

#include <boost/asio.hpp>

#include <chrono>
#include <ctime>
#include <set>
#include <utility>
#include <vector>

namespace xmreg
{
namespace
{
constexpr std::size_t maximum_response_bytes = 16 * 1024;
constexpr std::size_t maximum_dns_addresses = 8;

class bounded_http_client final
    : public epee::net_utils::http::http_simple_client
{
public:
    bool handle_target_data(std::string& piece_of_transfer) override
    {
        if (piece_of_transfer.size() > maximum_response_bytes - received_bytes_)
            return false;
        received_bytes_ += piece_of_transfer.size();
        return epee::net_utils::http::http_simple_client::handle_target_data(
                piece_of_transfer);
    }

private:
    std::size_t received_bytes_ {0};
};

struct endpoint_telemetry_response : cryptonote::rpc_response_base
{
    bool ready {false};
    std::string service_public_key;
    std::string descriptor_hash;
    bool telemetry_available {false};
    uint64_t telemetry_version {0};
    std::string software_version;
    uint64_t tip_height {0};
    std::string tip_hash;
    bool synchronized {false};

    BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_PARENT(cryptonote::rpc_response_base)
        KV_SERIALIZE(ready)
        KV_SERIALIZE(service_public_key)
        KV_SERIALIZE(descriptor_hash)
        KV_SERIALIZE_OPT(telemetry_available, false)
        KV_SERIALIZE_OPT(telemetry_version, (uint64_t)0)
        KV_SERIALIZE_OPT(software_version, std::string())
        KV_SERIALIZE_OPT(tip_height, (uint64_t)0)
        KV_SERIALIZE_OPT(tip_hash, std::string())
        KV_SERIALIZE_OPT(synchronized, false)
    END_KV_SERIALIZE_MAP()
};

bool resolve_public_endpoint(
        const std::string& host,
        uint64_t port,
        uint64_t transport,
        std::string& numeric_authority,
        bool& security_rejected,
        uint64_t timeout_ms)
{
    security_rejected = false;
    if (port == 0 || port > 65535 || transport < 1 || transport > 3)
    {
        security_rejected = true;
        return false;
    }

    boost::system::error_code error;
    if (transport == 1 || transport == 2)
    {
        const auto address = boost::asio::ip::address::from_string(host, error);
        if (error || (transport == 1 && !address.is_v4())
            || (transport == 2 && !address.is_v6())
            || !is_public_epose_address(address))
        {
            security_rejected = true;
            return false;
        }
        numeric_authority = address.is_v6()
            ? "[" + address.to_string() + "]:" + std::to_string(port)
            : address.to_string() + ":" + std::to_string(port);
        return true;
    }

    if (!valid_epose_dns_name(host))
    {
        security_rejected = true;
        return false;
    }

    boost::asio::io_context io;
    boost::asio::ip::tcp::resolver resolver(io);
    boost::asio::steady_timer deadline(io);
    boost::asio::ip::tcp::resolver::results_type results;
    bool completed {false};
    resolver.async_resolve(host, std::to_string(port),
            [&](const boost::system::error_code& resolve_error,
                boost::asio::ip::tcp::resolver::results_type resolved) {
                error = resolve_error;
                if (!error)
                    results = std::move(resolved);
                completed = true;
                deadline.cancel();
            });
    deadline.expires_after(std::chrono::milliseconds(timeout_ms));
    deadline.async_wait([&](const boost::system::error_code& timer_error) {
        if (!timer_error && !completed)
            resolver.cancel();
    });
    io.run();
    if (!completed || error)
        return false;

    std::set<std::string> addresses;
    for (const auto& result: results)
    {
        const auto address = result.endpoint().address();
        if (!is_public_epose_address(address))
        {
            security_rejected = true;
            return false;
        }
        addresses.emplace(address.to_string());
        if (addresses.size() > maximum_dns_addresses)
        {
            security_rejected = true;
            return false;
        }
    }
    if (addresses.empty())
        return false;

    const auto address = boost::asio::ip::address::from_string(*addresses.begin(), error);
    if (error)
        return false;
    numeric_authority = address.is_v6()
        ? "[" + address.to_string() + "]:" + std::to_string(port)
        : address.to_string() + ":" + std::to_string(port);
    return true;
}
}

epose_remote_telemetry fetch_epose_remote_telemetry(
        const std::string& host,
        uint64_t port,
        uint64_t transport,
        const std::string& descriptor_hash,
        const std::string& service_public_key,
        uint64_t timeout_ms)
{
    epose_remote_telemetry result;
    result.observed_at_unix = static_cast<uint64_t>(std::time(nullptr));

    std::string authority;
    bool security_rejected {false};
    if (!resolve_public_endpoint(
            host, port, transport, authority, security_rejected, timeout_ms))
    {
        result.state = security_rejected
            ? epose_telemetry_state::rejected
            : epose_telemetry_state::unreachable;
        return result;
    }

    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::request request;
    request.descriptor_hash = descriptor_hash;
    endpoint_telemetry_response response;
    bounded_http_client client;
    client.set_server("http://" + authority, {},
            epee::net_utils::ssl_support_t::e_ssl_support_disabled);
    const auto timeout = std::chrono::milliseconds(timeout_ms);
    if (!client.connect(timeout)
        || !epee::net_utils::invoke_http_json(
                "/get_epose_service_endpoint_v2", request, response, client, timeout))
        return result;

    if (response.status != CORE_RPC_STATUS_OK || !response.ready
        || response.descriptor_hash != descriptor_hash
        || response.service_public_key != service_public_key)
    {
        result.state = epose_telemetry_state::rejected;
        return result;
    }
    if (!response.telemetry_available)
    {
        result.state = epose_telemetry_state::not_reported;
        return result;
    }
    if (response.telemetry_version != 1
        || !valid_epose_software_version(response.software_version)
        || !is_lower_hex_telemetry_hash(response.tip_hash))
    {
        result.state = epose_telemetry_state::rejected;
        return result;
    }

    result.state = epose_telemetry_state::reported;
    result.telemetry_version = response.telemetry_version;
    result.software_version = response.software_version;
    result.tip_height = response.tip_height;
    result.tip_hash = response.tip_hash;
    result.synchronized = response.synchronized;
    return result;
}

}
