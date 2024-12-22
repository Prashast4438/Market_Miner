#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include <iomanip>
#include "websocket_server.hpp"
#include "market_data_handler.hpp"
#include "order_manager.hpp"
#include "logger.hpp"
#include "instrument_manager.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "nlohmann/json.hpp"
using json = nlohmann::json;
namespace net = boost::asio;

void print_header(const std::string& text) {
    std::cout << "\n\033[1;36m═══════════════════════ " << text << " ═══════════════════════\033[0m\n";
}

void print_subheader(const std::string& text) {
    std::cout << "\n\033[1;34m──── " << text << " ────\033[0m\n";
}

void print_success(const std::string& text) {
    std::cout << "\033[1;32m✓ " << text << "\033[0m\n";
}

void print_info(const std::string& label, const std::string& value) {
    std::cout << "\033[1;33m" << std::setw(20) << std::left << label << ":\033[0m " << value << "\n";
}

void print_json_summary(const json& j) {
    if (j.contains("result")) {
        const auto& result = j["result"];
        if (result.is_object()) {
            for (auto it = result.begin(); it != result.end(); ++it) {
                if (!it.value().is_object() && !it.value().is_array()) {
                    print_info(it.key(), it.value().dump());
                }
            }
        }
    }
}

int main() {
    try {
        spdlog::set_level(spdlog::level::info);
        print_header("DERIBIT TRADING SYSTEM");
        std::cout << "\n";

        // Test Order Management
        print_header("ORDER MANAGEMENT TEST");
        OrderManager order_manager(
            "b7lUgUcf",  // client_id
            "1zt_cUxrGaKHbsC8hK1DRQxnpsggaTC04XqxAV8etMI"  // client_secret
        );

        try {
            // Step 1: Get available instruments
            print_subheader("AVAILABLE INSTRUMENTS");
            json futures = order_manager.get_instruments("BTC", "future");
            json options = order_manager.get_instruments("BTC", "option");
            
            print_info("Futures Count", std::to_string(futures["result"].size()));
            print_info("Options Count", std::to_string(options["result"].size()));

            // Step 2: Get orderbook for BTC-PERPETUAL
            print_subheader("ORDERBOOK (BTC-PERPETUAL)");
            json orderbook = order_manager.get_orderbook("BTC-PERPETUAL");
            if (orderbook.contains("result")) {
                const auto& result = orderbook["result"];
                print_info("Best Bid", std::to_string(result["best_bid_price"].get<double>()) + " USD (" + 
                          std::to_string(result["best_bid_amount"].get<double>()) + " contracts)");
                print_info("Best Ask", std::to_string(result["best_ask_price"].get<double>()) + " USD (" + 
                          std::to_string(result["best_ask_amount"].get<double>()) + " contracts)");
                print_info("Mark Price", std::to_string(result["mark_price"].get<double>()));
                print_info("24h Volume", std::to_string(result["stats"]["volume"].get<double>()) + " BTC");
            }

            // Step 3: Get current positions
            print_subheader("CURRENT POSITIONS");
            json positions = order_manager.get_positions();
            if (!positions["result"].empty()) {
                for (const auto& pos : positions["result"]) {
                    print_info("Instrument", pos["instrument_name"].get<std::string>());
                    print_info("Size", std::to_string(pos["size"].get<double>()));
                    print_info("Entry Price", std::to_string(pos["average_price"].get<double>()));
                    print_info("Liquidation Price", pos["estimated_liquidation_price"].is_null() ? 
                             "N/A" : std::to_string(pos["estimated_liquidation_price"].get<double>()));
                }
            } else {
                print_info("Status", "No Open Positions");
            }

            // Step 4: Place a test limit order
            print_subheader("PLACING TEST ORDER");
            json order_response = order_manager.place_order(
                "BTC-PERPETUAL",  // instrument_name
                "buy",            // side
                100,             // amount
                40000.0,         // price
                "limit",         // type
                0.0,             // trigger_price
                "good_til_cancelled",  // time_in_force
                "test_order",    // label
                true,           // post_only
                false           // reduce_only
            );

            if (order_response.contains("result") && order_response["result"].contains("order")) {
                const auto& order = order_response["result"]["order"];
                print_success("Order Placed Successfully");
                print_info("Order ID", order["order_id"].get<std::string>());
                print_info("Type", order["direction"].get<std::string>() + " " + order["order_type"].get<std::string>());
                print_info("Size", std::to_string(order["amount"].get<double>()));
                print_info("Price", std::to_string(order["price"].get<double>()));

                std::string order_id = order["order_id"].get<std::string>();

                // Step 5: Modify the order
                print_subheader("MODIFYING ORDER");
                json modify_response = order_manager.edit_order(
                    order_id,
                    150,        // New amount
                    39000.0     // New price
                );

                if (modify_response.contains("result") && modify_response["result"].contains("order")) {
                    const auto& modified = modify_response["result"]["order"];
                    print_success("Order Modified Successfully");
                    print_info("New Size", std::to_string(modified["amount"].get<double>()));
                    print_info("New Price", std::to_string(modified["price"].get<double>()));
                }

                // Step 6: Cancel the order
                print_subheader("CANCELLING ORDER");
                json cancel_response = order_manager.cancel_order(order_id);
                
                if (cancel_response.contains("result")) {
                    print_success("Order Cancelled Successfully");
                    print_info("Final State", "Cancelled");
                }
            }

        } catch (const std::exception& e) {
            std::cout << "\033[1;31mError in order management: " << e.what() << "\033[0m\n";
        }

        print_header("TEST COMPLETED");

    } catch (const std::exception& e) {
        std::cout << "\033[1;31mFatal error: " << e.what() << "\033[0m\n";
        return 1;
    }

    return 0;
}
