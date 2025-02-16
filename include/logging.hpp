#pragma once

// Set the active log level to debug
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/details/synchronous_factory.h>
#include <memory>

namespace logging {

    // Windows console color codes
    namespace colors {
        constexpr uint16_t WHITE = 0x0007;
        constexpr uint16_t CYAN = 0x0003;
        constexpr uint16_t GREEN = 0x0002;
        constexpr uint16_t YELLOW = 0x0006;
        constexpr uint16_t RED = 0x0004;
        constexpr uint16_t BRIGHT_RED = 0x000C;
    }

    inline void init() {
        try {
            // Create colored console sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::debug);
            
            // Set colors for different levels
            console_sink->set_color(spdlog::level::trace, colors::WHITE);
            console_sink->set_color(spdlog::level::debug, colors::CYAN);
            console_sink->set_color(spdlog::level::info, colors::GREEN);
            console_sink->set_color(spdlog::level::warn, colors::YELLOW);
            console_sink->set_color(spdlog::level::err, colors::RED);
            console_sink->set_color(spdlog::level::critical, colors::BRIGHT_RED);
            
            // Create rotating file sink - 5MB max file size, 3 rotated files
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/digi-ellie.log", 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::trace);
            
            // Create logger with both sinks
            auto logger = std::make_shared<spdlog::logger>("digi-ellie", 
                spdlog::sinks_init_list{console_sink, file_sink});
            
            // Set global logging level to debug
            logger->set_level(spdlog::level::debug);
            
            // Set as default logger
            spdlog::set_default_logger(logger);
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
            
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }

} // namespace logging

// Logging macros
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__) 