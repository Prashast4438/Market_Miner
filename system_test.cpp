#include "order_manager.hpp"
#include "websocket_server.hpp"
#include "performance_monitor.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void print_json(const std::string& label, const json& data) {
    std::cout << "\n=== " << label << " ===\n";
    std::cout << data.dump(2) << std::endl;
}

int main() {
    try {
        // Initialize logging
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Starting full system test...");

        // Initialize OrderManager with API keys
        OrderManager order_manager("b7lUgUcf", "1zt_cUxrGaKHbsC8hK1DRQxnpsggaTC04XqxAV8etMI");

        // Initialize WebSocket server
        net::io_context ioc;
        auto ws_server = WebSocketServer::create(ioc, 8888);
        std::thread ioc_thread([&ioc](){ ioc.run(); });

        // 1. Test Market Data
        spdlog::info("Testing market data retrieval...");
        auto orderbook = order_manager.get_orderbook("BTC-PERPETUAL");
        print_json("Orderbook for BTC-PERPETUAL", orderbook);

        // 2. Test Position Management
        spdlog::info("Testing position retrieval...");
        auto positions = order_manager.get_positions();
        print_json("Current Positions", positions);

        // 3. Test Order Management
        spdlog::info("Testing order placement...");
        
        // Place a limit buy order
        auto buy_order = order_manager.place_order(
            "BTC-PERPETUAL",  // instrument
            "buy",            // side
            10,              // amount (contracts)
            25000.0,         // price (USD)
            "limit"          // type
        );
        print_json("Placed Buy Order", buy_order);

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Modify the order
        if (buy_order.contains("order") && buy_order["order"].contains("order_id")) {
            std::string order_id = buy_order["order"]["order_id"];
            
            spdlog::info("Testing order modification...");
            auto modified_order = order_manager.edit_order(
                order_id,
                10,        // same amount
                25100.0    // new price
            );
            print_json("Modified Order", modified_order);

            // Wait a bit
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Cancel the order
            spdlog::info("Testing order cancellation...");
            auto cancelled_order = order_manager.cancel_order(order_id);
            print_json("Cancelled Order", cancelled_order);
        }

        // 4. Test WebSocket Performance
        spdlog::info("Testing WebSocket performance...");
        
        // Create a test client
        std::thread client_thread([&]() {
            // Wait for server to start
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Connect to WebSocket server
            net::io_context client_ioc;
            tcp::resolver resolver(client_ioc);
            auto const results = resolver.resolve("127.0.0.1", "8888");

            websocket::stream<tcp::socket> ws{client_ioc};
            auto ep = net::connect(ws.next_layer(), results);
            ws.handshake("127.0.0.1", "/");

            // Subscribe to a symbol
            json sub_msg = {
                {"action", "subscribe"},
                {"symbol", "BTC-PERPETUAL"}
            };
            ws.write(net::buffer(sub_msg.dump()));

            // Read some messages
            beast::flat_buffer buffer;
            for (int i = 0; i < 5; ++i) {
                ws.read(buffer);
                std::cout << "Received: " << beast::buffers_to_string(buffer.data()) << std::endl;
                buffer.consume(buffer.size());
            }

            // Close gracefully
            ws.close(websocket::close_code::normal);
        });

        // Let the test run for a while
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Print performance metrics
        auto& monitor = PerformanceMonitor::instance();
        monitor.print_metrics();

        // Cleanup
        ioc.stop();
        if (ioc_thread.joinable()) ioc_thread.join();
        if (client_thread.joinable()) client_thread.join();

        spdlog::info("System test completed successfully!");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("System test failed: {}", e.what());
        return 1;
    }
}
