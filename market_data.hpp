#pragma once
#include "websocket_server.hpp"

class MarketDataHandler : public std::enable_shared_from_this<MarketDataHandler> {
public:
    explicit MarketDataHandler(WebSocketServer& server, net::io_context& ioc) 
        : ws_server_(server)
        , ioc_(ioc)
        , resolver_(ioc)
        , ws_stream_(ioc) {
        
        // Set up the stream
        ws_stream_.binary(true);
        ws_stream_.write_buffer_bytes(8192);
    }

    void start() {
        // Start connection
        connect_to_deribit();
    }

    ~MarketDataHandler() {
        if(ws_stream_.is_open()) {
            beast::error_code ec;
            ws_stream_.close(websocket::close_code::normal, ec);
            if(ec) {
                spdlog::error("Error closing Deribit connection: {}", ec.message());
            }
        }
    }

private:
    void connect_to_deribit() {
        // Resolve the Deribit endpoint
        resolver_.async_resolve(
            "test.deribit.com",
            "443",
            beast::bind_front_handler(
                &MarketDataHandler::on_resolve,
                shared_from_this()));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if(ec) {
            spdlog::error("Resolve failed: {}", ec.message());
            return;
        }

        // Connect to the IP address we get from a lookup
        beast::get_lowest_layer(ws_stream_).async_connect(
            results,
            beast::bind_front_handler(
                &MarketDataHandler::on_connect,
                shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        if(ec) {
            spdlog::error("Connect failed: {}", ec.message());
            return;
        }

        // Perform the websocket handshake
        ws_stream_.async_handshake(
            "test.deribit.com",
            "/ws/api/v2",
            beast::bind_front_handler(
                &MarketDataHandler::on_handshake,
                shared_from_this()));
    }

    void on_handshake(beast::error_code ec) {
        if(ec) {
            spdlog::error("WebSocket handshake failed: {}", ec.message());
            return;
        }

        spdlog::info("Connected to Deribit WebSocket");
        subscribe_to_market_data();
        do_read();
    }

    void subscribe_to_market_data() {
        try {
            // Subscribe to orderbook updates for BTC-PERPETUAL
            json subscribe_msg = {
                {"jsonrpc", "2.0"},
                {"method", "public/subscribe"},
                {"params", {
                    "channels", {
                        "book.BTC-PERPETUAL.100ms"
                    }
                }},
                {"id", 42}
            };

            ws_stream_.async_write(
                net::buffer(subscribe_msg.dump()),
                beast::bind_front_handler(
                    &MarketDataHandler::on_write,
                    shared_from_this()));

            spdlog::info("Sent subscription request to Deribit");

        } catch (const std::exception& e) {
            spdlog::error("Failed to subscribe to market data: {}", e.what());
        }
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        if(ec) {
            spdlog::error("Write failed: {}", ec.message());
            return;
        }
    }

    void do_read() {
        ws_stream_.async_read(
            read_buffer_,
            beast::bind_front_handler(
                &MarketDataHandler::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        if(ec) {
            if(ec != websocket::error::closed) {
                spdlog::error("Read failed: {}", ec.message());
            }
            return;
        }

        try {
            std::string msg = beast::buffers_to_string(read_buffer_.data());
            read_buffer_.consume(read_buffer_.size());

            auto data = json::parse(msg);
            
            // Handle subscription confirmation
            if (data.contains("id") && data["id"] == 42) {
                if (data.contains("error")) {
                    spdlog::error("Subscription failed: {}", data["error"]["message"].get<std::string>());
                    return;
                }
                spdlog::info("Successfully subscribed to market data");
            }
            // Handle market data updates
            else if (data.contains("method") && data["method"] == "subscription") {
                if (data.contains("params") && data["params"].contains("channel")) {
                    std::string channel = data["params"]["channel"];
                    
                    // Format market data for clients
                    json client_msg = {
                        {"type", "market_data"},
                        {"symbol", "BTC-PERPETUAL"},
                        {"data", data["params"]["data"]}
                    };

                    // Broadcast to all connected clients
                    ws_server_.broadcast(client_msg.dump());
                }
            }

        } catch (const std::exception& e) {
            spdlog::error("Error processing Deribit message: {}", e.what());
        }

        // Queue up another read
        do_read();
    }

    WebSocketServer& ws_server_;
    net::io_context& ioc_;
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_stream_;
    beast::flat_buffer read_buffer_;
};
