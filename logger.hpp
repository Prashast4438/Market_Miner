#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class Logger {
public:
    static void init() {
        try {
            // Create console sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);

            // Create rotating file sink - 5MB size, 3 rotated files
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/deribit_trading.log", 5*1024*1024, 3);
            file_sink->set_level(spdlog::level::debug);

            // Create logger with both sinks
            auto logger = std::make_shared<spdlog::logger>("deribit_trader",
                spdlog::sinks_init_list({console_sink, file_sink}));
            
            // Set pattern
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            
            // Set as default logger
            spdlog::set_default_logger(logger);
            spdlog::info("Logger initialized");
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }
};
