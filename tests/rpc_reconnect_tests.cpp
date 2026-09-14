#include "src/rpccalls.h"

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace
{
using tcp = boost::asio::ip::tcp;

class stale_keepalive_server
{
    boost::asio::io_context io_;
    tcp::acceptor acceptor_;
    std::thread worker_;
    std::atomic<unsigned> connections_ {0};

    static void read_request(tcp::socket& socket)
    {
        boost::asio::streambuf request;
        boost::asio::read_until(socket, request, "\r\n\r\n");
        std::istream input(&request);
        std::string line;
        std::size_t content_length {0};
        while (std::getline(input, line) && line != "\r")
        {
            const std::string prefix = "Content-Length:";
            if (line.compare(0, prefix.size(), prefix) == 0)
                content_length = std::stoul(line.substr(prefix.size()));
        }
        const std::size_t buffered = request.size();
        if (buffered < content_length)
            boost::asio::read(socket, request,
                    boost::asio::transfer_exactly(content_length - buffered));
    }

    static void send_response(tcp::socket& socket)
    {
        const std::string body =
                R"({"jsonrpc":"2.0","id":0,"result":{"status":"OK","height":7,"target_height":7,"target":120,"testnet":false,"stagenet":false}})";
        const std::string response =
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Content-Length: " + std::to_string(body.size())
                + "\r\nConnection: keep-alive\r\n\r\n" + body;
        boost::asio::write(socket, boost::asio::buffer(response));
    }

public:
    stale_keepalive_server()
        : acceptor_(io_, tcp::endpoint(tcp::v4(), 0))
    {}

    ~stale_keepalive_server()
    {
        boost::system::error_code ignored;
        acceptor_.close(ignored);
        if (worker_.joinable())
            worker_.join();
    }

    uint16_t port() const
    {
        return acceptor_.local_endpoint().port();
    }

    unsigned connections() const
    {
        return connections_.load();
    }

    void start()
    {
        worker_ = std::thread([this]() {
            for (unsigned request = 0; request < 2; ++request)
            {
                tcp::socket socket(io_);
                boost::system::error_code error;
                acceptor_.accept(socket, error);
                if (error)
                    return;
                ++connections_;
                read_request(socket);
                send_response(socket);
                boost::system::error_code ignored;
                socket.shutdown(tcp::socket::shutdown_both, ignored);
                socket.close(ignored);
            }
        });
    }
};
}

int main()
{
    stale_keepalive_server server;
    server.start();

    xmreg::rpccalls rpc("http://127.0.0.1:" + std::to_string(server.port()),
                        xmreg::rpccalls::login_opt {}, 2000);
    cryptonote::COMMAND_RPC_GET_INFO::response first {};
    cryptonote::COMMAND_RPC_GET_INFO::response second {};
    if (!rpc.get_network_info(first))
    {
        std::cerr << "FAILED: initial keep-alive request\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!rpc.get_network_info(second))
    {
        std::cerr << "FAILED: stale connection did not reconnect\n";
        return 1;
    }
    if (server.connections() != 2 || second.height != 7 || second.target != 120)
    {
        std::cerr << "FAILED: reconnect response was not validated\n";
        return 1;
    }
    return 0;
}
