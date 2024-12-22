#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include "websocket_server.hpp"
#include "order_manager.hpp"
#include "performance_monitor.hpp"

using namespace std::chrono_literals;

// Test credentials (replace with your test account credentials)
const std::string CLIENT_ID = "b7lUgUcf";
const std::string CLIENT_SECRET = "1zt_cUxrGaKHbsC8hK1DRQxnpsggaTC04XqxAV8etMI";
const uint16_t SERVER_PORT = 8888;  // Changed port to avoid conflicts

void run_performance_test() {
    try {
        boost::asio::io_context ioc;
        auto server = WebSocketServer::create(ioc, SERVER_PORT);
        auto order_manager = std::make_shared<OrderManager>(CLIENT_ID, CLIENT_SECRET);

        spdlog::info("WebSocket server started on port {}", SERVER_PORT);

        // Start WebSocket server in a separate thread
        std::thread server_thread([&ioc]() {
            try {
                ioc.run();
            } catch (const std::exception& e) {
                spdlog::error("Server thread error: {}", e.what());
            }
        });

        // Connect test clients
        const int NUM_CLIENTS = 100;
        const int MESSAGES_PER_CLIENT = 1000;
        std::vector<std::thread> client_threads;

        auto client_func = [](int client_id, int num_messages) {
            try {
                boost::asio::io_context ioc;
                tcp::resolver resolver(ioc);
                auto const results = resolver.resolve("localhost", std::to_string(SERVER_PORT));

                websocket::stream<tcp::socket> ws{ioc};
                auto ep = boost::asio::connect(ws.next_layer(), results);
                ws.handshake("localhost", "/");

                // Subscribe to market data
                json sub_msg = {
                    {"action", "subscribe"},
                    {"symbol", "BTC-PERPETUAL"}
                };
                ws.write(boost::asio::buffer(sub_msg.dump()));

                // Measure message latency
                for (int i = 0; i < num_messages; ++i) {
                    auto start = std::chrono::high_resolution_clock::now();
                    
                    // Send test message
                    json test_msg = {
                        {"type", "test"},
                        {"client_id", client_id},
                        {"message_id", i}
                    };
                    ws.write(boost::asio::buffer(test_msg.dump()));

                    // Read response
                    beast::flat_buffer buffer;
                    ws.read(buffer);

                    auto end = std::chrono::high_resolution_clock::now();
                    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                        end - start).count() / 1000.0;

                    PerformanceMonitor::instance().track_websocket_latency(
                        "client_" + std::to_string(client_id), latency);

                    std::this_thread::sleep_for(10ms);  // Rate limiting
                }

                ws.close(websocket::close_code::normal);
            } catch (const std::exception& e) {
                spdlog::error("Client {} error: {}", client_id, e.what());
            }
        };

        // Start client threads
        spdlog::info("Starting {} test clients...", NUM_CLIENTS);
        for (int i = 0; i < NUM_CLIENTS; ++i) {
            client_threads.emplace_back(client_func, i, MESSAGES_PER_CLIENT);
            std::this_thread::sleep_for(50ms);  // Stagger client connections
        }

        // Wait for all clients to finish
        for (auto& thread : client_threads) {
            thread.join();
        }

        // Print performance metrics
        spdlog::info("Performance test completed. Generating metrics...");
        PerformanceMonitor::instance().print_detailed_metrics();

        // Cleanup
        ioc.stop();
        server_thread.join();

    } catch (const std::exception& e) {
        spdlog::error("Performance test error: {}", e.what());
    }
}

int main() {
    try {
        spdlog::info("Starting performance test...");
        run_performance_test();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}
