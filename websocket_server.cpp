#include "websocket_server.hpp"

WebSocketServer::WebSocketServer(net::io_context& ioc, uint16_t port)
    : ioc_(ioc)
    , acceptor_(ioc, {net::ip::make_address("0.0.0.0"), port}) {
    spdlog::info("WebSocket server starting on port {}", port);
}

void WebSocketServer::start() {
    do_accept();
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(
            &WebSocketServer::on_accept,
            shared_from_this()));
}

void WebSocketServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        spdlog::error("Accept failed: {}", ec.message());
    } else {
        std::make_shared<WebSocketSession>(
            std::move(socket),
            shared_from_this()
        )->start();
    }
    do_accept();
}

WebSocketSession::WebSocketSession(tcp::socket&& socket, std::shared_ptr<WebSocketServer> server)
    : ws_(std::move(socket))
    , server_(server)
    , message_queue_(MESSAGE_QUEUE_SIZE) {
    // Configure WebSocket for optimal performance
    ws_.binary(true);  // Use binary mode for better performance
    ws_.auto_fragment(false);  // Disable auto-fragmentation
    ws_.write_buffer_bytes(BUFFER_SIZE);  // Set write buffer size
    ws_.read_message_max(BUFFER_SIZE);  // Set max message size
}

void WebSocketSession::start() {
    ws_.async_accept(
        beast::bind_front_handler(
            &WebSocketSession::on_accept,
            shared_from_this()));
}

void WebSocketSession::on_accept(beast::error_code ec) {
    if (ec) {
        spdlog::error("WebSocket accept failed: {}", ec.message());
        return;
    }
    do_read();
    process_message_queue();
}

void WebSocketSession::do_read() {
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &WebSocketSession::on_read,
            shared_from_this()));
}

void WebSocketSession::handle_subscription(const json& msg) {
    try {
        if (!msg.contains("action") || !msg.contains("symbol")) {
            return;
        }

        std::string action = msg["action"];
        std::string symbol = msg["symbol"];

        if (action == "subscribe") {
            server_->subscriptions_[shared_from_this()].insert(symbol);
            spdlog::info("Client subscribed to {}", symbol);
        } else if (action == "unsubscribe") {
            server_->subscriptions_[shared_from_this()].erase(symbol);
            spdlog::info("Client unsubscribed from {}", symbol);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling subscription: {}", e.what());
    }
}

void WebSocketSession::send(const std::string& message) {
    WebSocketMessage msg;
    if (message.size() > BUFFER_SIZE) {
        spdlog::warn("Message too large: {} bytes", message.size());
        return;
    }

    std::copy(message.begin(), message.end(), msg.data.begin());
    msg.size = message.size();
    msg.timestamp = std::chrono::high_resolution_clock::now();

    if (!message_queue_.push(msg)) {
        spdlog::warn("Message queue full, dropping message");
        return;
    }

    if (!is_sending_) {
        process_message_queue();
    }
}

void WebSocketSession::handle_market_data(const std::string& symbol, const json& data) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        json message = {
            {"symbol", symbol},
            {"data", data},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };
        send(message.dump());
    } catch (const std::exception& e) {
        spdlog::error("Error handling market data: {}", e.what());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count() / 1000.0;
    PerformanceMonitor::instance().track_websocket_latency("market_data_" + symbol, duration);
}

void WebSocketSession::process_message_queue() {
    if (is_sending_.exchange(true)) {
        return;  // Already processing
    }

    WebSocketMessage msg;
    if (message_queue_.pop(msg)) {
        auto now = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            now - msg.timestamp).count() / 1000.0;
        
        PerformanceMonitor::instance().track_websocket_latency("queue_latency", latency);

        ws_.async_write(
            net::buffer(msg.data.data(), msg.size),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                self->is_sending_ = false;
                if (!ec) {
                    self->process_message_queue();
                }
            });
    } else {
        is_sending_ = false;
    }
}

void WebSocketSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec != websocket::error::closed) {
            spdlog::error("WebSocket read failed: {}", ec.message());
        }
        return;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Process the message
    std::string payload = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());  // Clear the buffer

    try {
        json msg = json::parse(payload);
        handle_subscription(msg);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse message: {}", e.what());
    }

    // Track message processing latency
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count() / 1000.0;
    PerformanceMonitor::instance().track_websocket_latency("message_processing", duration);

    do_read();
}
