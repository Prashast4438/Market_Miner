#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <map>
#include <curl/curl.h>

using json = nlohmann::json;

class OrderManager {
public:
    explicit OrderManager(const std::string& client_id, const std::string& client_secret)
        : client_id_(client_id)
        , client_secret_(client_secret)
        , access_token_("") {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        authenticate();
    }

    ~OrderManager() {
        curl_global_cleanup();
    }

    // Place a new order
    json place_order(const std::string& instrument_name,
                    const std::string& side,
                    int amount,
                    double price,
                    const std::string& type = "limit",  // "limit", "market", "stop_limit", "stop_market"
                    double trigger_price = 0.0,         // For stop orders
                    const std::string& time_in_force = "good_til_cancelled",
                    const std::string& label = "",
                    bool post_only = false,
                    bool reduce_only = false) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Create the order request
        json params = {
            {"instrument_name", instrument_name},
            {"amount", amount},
            {"type", type}
        };

        // Add optional parameters
        if (price != 0.0 && type != "market") {
            params["price"] = price;
        }
        
        if (trigger_price != 0.0 && (type == "stop_limit" || type == "stop_market")) {
            params["trigger_price"] = trigger_price;
        }

        if (!label.empty()) {
            params["label"] = label;
        }

        params["time_in_force"] = time_in_force;
        params["post_only"] = post_only;
        params["reduce_only"] = reduce_only;

        // Log the request parameters
        std::string endpoint = "private/" + side;
        spdlog::debug("Sending order request to {}", endpoint);
        spdlog::debug("Request params: {}", params.dump(2));

        // Send the request
        json response = make_authenticated_request(endpoint, params);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        spdlog::info("Order placement latency: {} microseconds", duration.count());
        
        return response;
    }

    // Cancel an order
    json cancel_order(const std::string& order_id) {
        auto start_time = std::chrono::high_resolution_clock::now();

        json params = {
            {"order_id", order_id}
        };

        json response = make_authenticated_request(
            "private/cancel",
            params
        );

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        spdlog::info("Order cancellation latency: {} microseconds", duration.count());
        
        return response;
    }

    // Modify an existing order
    json edit_order(const std::string& order_id,
                   int amount,
                   double price) {
        auto start_time = std::chrono::high_resolution_clock::now();

        json params = {
            {"order_id", order_id},
            {"amount", amount},
            {"price", price}
        };

        json response = make_authenticated_request(
            "private/edit",
            params
        );

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        spdlog::info("Order modification latency: {} microseconds", duration.count());
        
        return response;
    }

    // Get current positions
    json get_positions(const std::string& currency = "BTC") {
        auto start_time = std::chrono::high_resolution_clock::now();

        json params = {
            {"currency", currency}
        };

        json response = make_authenticated_request(
            "private/get_positions",
            params
        );

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        spdlog::info("Position query latency: {} microseconds", duration.count());
        
        return response;
    }

    // Get orderbook for an instrument
    json get_orderbook(const std::string& instrument_name, int depth = 20) {
        auto start_time = std::chrono::high_resolution_clock::now();

        json params = {
            {"instrument_name", instrument_name},
            {"depth", depth}
        };

        json response = make_authenticated_request(
            "public/get_order_book",
            params
        );

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        spdlog::info("Orderbook query latency: {} microseconds", duration.count());
        
        return response;
    }

    // Get all available instruments of a specific type
    json get_instruments(const std::string& currency = "BTC", 
                        const std::string& kind = "future",  // "future", "option", or "spot"
                        bool expired = false) {
        auto start_time = std::chrono::high_resolution_clock::now();

        json params = {
            {"currency", currency},
            {"kind", kind},
            {"expired", expired}
        };

        json response = make_authenticated_request(
            "public/get_instruments",
            params
        );

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        spdlog::info("Instrument query latency: {} microseconds", duration.count());
        
        return response;
    }

private:
    static size_t curl_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    void authenticate() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }

        std::string url = "https://test.deribit.com/api/v2/public/auth";
        
        // Create the auth request with OAuth2 parameters
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "public/auth"},
            {"params", {
                {"grant_type", "client_credentials"},
                {"client_id", client_id_},
                {"client_secret", client_secret_},
                {"scope", "mainaccount"}
            }}
        };

        std::string post_data = request.dump();
        std::string response_string;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        spdlog::debug("Sending authentication request to {}", url);
        spdlog::debug("Request payload: {}", post_data);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (res != CURLE_OK) {
            spdlog::error("Authentication failed: CURL error: {}", curl_easy_strerror(res));
            throw std::runtime_error("Authentication failed: " + std::string(curl_easy_strerror(res)));
        }

        try {
            spdlog::debug("Raw authentication response: {}", response_string);
            auto response = json::parse(response_string);
            
            if (response.contains("error")) {
                const auto& error = response["error"];
                std::string error_message = error.contains("message") ? 
                    error["message"].get<std::string>() : "Unknown error";
                int error_code = error.contains("code") ? 
                    error["code"].get<int>() : -1;
                
                spdlog::error("Authentication failed: {} (code: {})", error_message, error_code);
                throw std::runtime_error("Authentication failed: " + error_message);
            }
            
            if (!response.contains("result") || !response["result"].contains("access_token")) {
                spdlog::error("Invalid authentication response: {}", response.dump());
                throw std::runtime_error("Invalid authentication response");
            }

            access_token_ = response["result"]["access_token"];
            // Store token expiry time
            if (response["result"].contains("expires_in")) {
                token_expiry_ = std::chrono::system_clock::now() + 
                    std::chrono::seconds(response["result"]["expires_in"].get<int>());
            }
            
            spdlog::info("Successfully authenticated with Deribit (latency: {} μs)", duration.count());
            
        } catch (const json::parse_error& e) {
            spdlog::error("Failed to parse authentication response: {}", response_string);
            throw std::runtime_error("Failed to parse authentication response: " + std::string(e.what()));
        }
    }

    json make_authenticated_request(const std::string& method, const json& params) {
        // Check if token is expired or about to expire (within 30 seconds)
        auto now = std::chrono::system_clock::now();
        if (access_token_.empty() || now + std::chrono::seconds(30) >= token_expiry_) {
            authenticate();
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }

        std::string url = "https://test.deribit.com/api/v2/" + method;
        
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},  // Use a simple numeric ID
            {"method", method},  // Use the full method path
            {"params", params}
        };

        std::string post_data = request.dump();
        std::string response_string;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token_).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        if (res != CURLE_OK) {
            spdlog::error("Request failed: {} (latency: {} μs)", curl_easy_strerror(res), duration.count());
            throw std::runtime_error("Request failed: " + std::string(curl_easy_strerror(res)));
        }

        try {
            auto response = json::parse(response_string);
            
            // Handle various error cases
            if (response.contains("error")) {
                const auto& error = response["error"];
                std::string error_message = error.contains("message") ? 
                    error["message"].get<std::string>() : "Unknown error";
                int error_code = error.contains("code") ? 
                    error["code"].get<int>() : -1;
                
                spdlog::error("API error: {} (code: {}, latency: {} μs)", 
                    error_message, error_code, duration.count());
                spdlog::debug("Full API response: {}", response.dump(2));
                
                // Handle token expiration
                if (error_code == 13009) {  // Token expired
                    spdlog::info("Token expired, re-authenticating...");
                    authenticate();
                    return make_authenticated_request(method, params);  // Retry with new token
                }
                
                throw std::runtime_error("API error: " + error_message);
            }

            spdlog::debug("Request successful (method: {}, latency: {} μs)", method, duration.count());
            return response;
            
        } catch (const json::parse_error& e) {
            spdlog::error("Failed to parse response: {} (latency: {} μs)", e.what(), duration.count());
            throw std::runtime_error("Failed to parse response: " + std::string(e.what()));
        }
    }

    std::string client_id_;
    std::string client_secret_;
    std::string access_token_;
    std::chrono::system_clock::time_point token_expiry_;
};
