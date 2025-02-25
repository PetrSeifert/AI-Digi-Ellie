#include "whisper_service.hpp"
#include "logging.hpp"
#include "config.hpp"
#include <iostream>
#include <csignal>
#include <filesystem>

std::unique_ptr<WhisperService> service;

void signalHandler(int signum) {
    LOG_INFO("Signal {} received, stopping service...", signum);
    if (service) {
        service->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // Get configuration from environment
        std::string host = config::WHISPER_SERVICE_HOST;
        int port = config::WHISPER_SERVICE_PORT;
        std::filesystem::path model_path = std::filesystem::path("whisper_models") / config::WHISPER_MODEL_NAME;

        LOG_INFO("Starting Whisper service with model: {}", model_path.string());
        LOG_INFO("Listening on {}:{}", host, port);

        // Create and start the service - Convert path to string
        service = std::make_unique<WhisperService>(model_path.string(), host, port);
        service->start();

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start Whisper service: {}", e.what());
        return 1;
    }

    return 0;
} 