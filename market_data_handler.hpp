#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <set>
#include <map>
#include <chrono>
#include "performance_monitor.hpp"

using json = nlohmann::json;

class MarketDataHandler {
public:
    MarketDataHandler() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        start_update_thread();
    }

    ~MarketDataHandler() {
        stop_update_thread();
        curl_global_cleanup();
    }

    json get_orderbook(const std::string& instrument_name) {
        auto& perf = PerformanceMonitor::instance();
        perf.start_operation("get_orderbook");

        std::string url = "https://test.deribit.com/api/v2/public/get_order_book?instrument_name=" + instrument_name;
        std::string response = make_request(url);
        
        perf.end_operation("get_orderbook");
        return json::parse(response);
    }

    json get_ticker(const std::string& instrument_name) {
        auto& perf = PerformanceMonitor::instance();
        perf.start_operation("get_ticker");

        std::string url = "https://test.deribit.com/api/v2/public/ticker?instrument_name=" + instrument_name;
        std::string response = make_request(url);
        
        perf.end_operation("get_ticker");
        return json::parse(response);
    }

    json get_instruments(const std::string& currency, const std::string& kind) {
        auto& perf = PerformanceMonitor::instance();
        perf.start_operation("get_instruments");

        std::string url = "https://test.deribit.com/api/v2/public/get_instruments?currency=" + currency + "&kind=" + kind;
        std::string response = make_request(url);
        
        perf.end_operation("get_instruments");
        return json::parse(response);
    }

private:
    std::atomic<bool> running_{false};
    std::thread update_thread_;
    std::mutex mutex_;
    std::map<std::string, json> latest_orderbooks_;
    std::set<std::string> active_instruments_;

    static size_t curl_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string make_request(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        }

        return response;
    }

    void start_update_thread() {
        running_ = true;
        update_thread_ = std::thread([this]() {
            while (running_) {
                try {
                    auto& perf = PerformanceMonitor::instance();
                    perf.start_operation("market_data_update");

                    // Update orderbooks for all active instruments
                    std::set<std::string> instruments;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        instruments = active_instruments_;
                    }

                    for (const auto& instrument : instruments) {
                        try {
                            json orderbook = get_orderbook(instrument);
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                latest_orderbooks_[instrument] = orderbook;
                            }
                        } catch (const std::exception& e) {
                            spdlog::error("Failed to update orderbook for {}: {}", instrument, e.what());
                        }
                    }

                    perf.end_operation("market_data_update");
                } catch (const std::exception& e) {
                    spdlog::error("Error in market data update thread: {}", e.what());
                }

                // Sleep for a short duration before next update
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void stop_update_thread() {
        running_ = false;
        if (update_thread_.joinable()) {
            update_thread_.join();
        }
    }
};
