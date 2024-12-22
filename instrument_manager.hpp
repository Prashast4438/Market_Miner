#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "market_data_handler.hpp"
#include "performance_monitor.hpp"

using json = nlohmann::json;

enum class InstrumentType {
    SPOT,
    FUTURES,
    OPTION
};

struct Instrument {
    std::string name;
    InstrumentType type;
    std::string base_currency;
    std::string quote_currency;
    double tick_size;
    double min_trade_amount;
    bool is_active;
    
    // Additional fields for futures
    std::string settlement_period;  // perpetual, month, quarter
    double index_price;
    
    // Additional fields for options
    double strike;
    std::string option_type;  // "call" or "put"
    std::string expiration_timestamp;
};

class InstrumentManager {
public:
    InstrumentManager() {
        refresh_instruments();
    }

    std::vector<Instrument> get_instruments(InstrumentType type, const std::string& currency = "BTC") {
        auto& perf = PerformanceMonitor::instance();
        perf.start_operation("get_instruments");

        std::vector<Instrument> result;
        std::string kind;
        
        switch (type) {
            case InstrumentType::SPOT:
                kind = "spot";
                break;
            case InstrumentType::FUTURES:
                kind = "future";
                break;
            case InstrumentType::OPTION:
                kind = "option";
                break;
        }

        try {
            auto market_data = std::make_shared<MarketDataHandler>();
            json response = market_data->get_instruments(currency, kind);
            
            if (response.contains("result")) {
                for (const auto& instr : response["result"]) {
                    Instrument instrument;
                    instrument.name = instr["instrument_name"].get<std::string>();
                    instrument.base_currency = instr["base_currency"].get<std::string>();
                    instrument.quote_currency = instr["quote_currency"].get<std::string>();
                    instrument.tick_size = instr["tick_size"].get<double>();
                    instrument.min_trade_amount = instr["min_trade_amount"].get<double>();
                    instrument.is_active = instr["is_active"].get<bool>();
                    instrument.type = type;

                    if (type == InstrumentType::FUTURES) {
                        instrument.settlement_period = instr["settlement_period"].get<std::string>();
                        if (instr.contains("index_price")) {
                            instrument.index_price = instr["index_price"].get<double>();
                        }
                    } else if (type == InstrumentType::OPTION) {
                        instrument.strike = instr["strike"].get<double>();
                        instrument.option_type = instr["option_type"].get<std::string>();
                        instrument.expiration_timestamp = instr["expiration_timestamp"].get<std::string>();
                    }

                    result.push_back(instrument);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to get instruments: {}", e.what());
        }

        perf.end_operation("get_instruments");
        return result;
    }

    void refresh_instruments() {
        auto& perf = PerformanceMonitor::instance();
        perf.start_operation("refresh_instruments");

        try {
            // Refresh BTC instruments
            refresh_currency_instruments("BTC");
            // Refresh ETH instruments
            refresh_currency_instruments("ETH");
        } catch (const std::exception& e) {
            spdlog::error("Failed to refresh instruments: {}", e.what());
        }

        perf.end_operation("refresh_instruments");
    }

private:
    void refresh_currency_instruments(const std::string& currency) {
        auto market_data = std::make_shared<MarketDataHandler>();
        
        // Get spot instruments
        json spot_response = market_data->get_instruments(currency, "spot");
        process_instruments(spot_response, InstrumentType::SPOT);
        
        // Get futures instruments
        json futures_response = market_data->get_instruments(currency, "future");
        process_instruments(futures_response, InstrumentType::FUTURES);
        
        // Get options instruments
        json options_response = market_data->get_instruments(currency, "option");
        process_instruments(options_response, InstrumentType::OPTION);
    }

    void process_instruments(const json& response, InstrumentType type) {
        if (!response.contains("result")) {
            return;
        }

        for (const auto& instr : response["result"]) {
            std::string name = instr["instrument_name"].get<std::string>();
            instruments_[name] = instr;
            instrument_types_[name] = type;
        }
    }

    std::map<std::string, json> instruments_;
    std::map<std::string, InstrumentType> instrument_types_;
};
