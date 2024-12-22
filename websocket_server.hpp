#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <array>
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include "performance_monitor.hpp"

using json = nlohmann::json;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Pre-allocated buffer size for WebSocket messages
constexpr size_t BUFFER_SIZE = 16384;  // 16KB
constexpr size_t MESSAGE_QUEUE_SIZE = 1024;

// Forward declarations
class WebSocketServer;
class WebSocketSession;

// Thread-safe message queue for WebSocket messages
struct WebSocketMessage {
    std::array<char, BUFFER_SIZE> data;
    size_t size;
    std::chrono::high_resolution_clock::time_point timestamp;
};

// Represents a single WebSocket connection with optimizations
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket&& socket, std::shared_ptr<WebSocketServer> server);
    void start();

private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_{BUFFER_SIZE};  // Pre-allocated buffer
    std::shared_ptr<WebSocketServer> server_;
    std::string session_id_;
    boost::lockfree::spsc_queue<WebSocketMessage> message_queue_;
    std::atomic<bool> is_sending_{false};

    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void process_message_queue();
    void handle_subscription(const json& msg);
    void send(const std::string& message);
    void handle_market_data(const std::string& symbol, const json& data);
};

// Manages WebSocket connections with thread pool
class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
public:
    static std::shared_ptr<WebSocketServer> create(net::io_context& ioc, uint16_t port) {
        auto server = std::shared_ptr<WebSocketServer>(new WebSocketServer(ioc, port));
        server->start();
        return server;
    }

    std::map<std::shared_ptr<WebSocketSession>, std::set<std::string>> subscriptions_;

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

    explicit WebSocketServer(net::io_context& ioc, uint16_t port);
    void start();
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    friend class WebSocketSession;
};
