#include "whisper_service.hpp"
#include "logging.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WhisperService::WhisperService(const std::string& model_path, const std::string& host, int port)
    : host(host), port(port), running(false) {
    
    whisper = std::make_unique<WhisperSTT>(model_path);
    server = std::make_unique<httplib::Server>();
    
    setupRoutes();
}

WhisperService::~WhisperService() {
    stop();
}

void WhisperService::setupRoutes() {
    // Health check endpoint
    server->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    // Transcription endpoint
    server->Post("/transcribe", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_header("Content-Type") || req.get_header_value("Content-Type") != "audio/raw") {
            res.status = 400;
            json error = {{"error", "Invalid content type. Expected audio/raw"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            // Convert body to vector of bytes
            std::vector<uint8_t> audio_data(req.body.begin(), req.body.end());
            
            // Process the audio
            std::string transcription = handleTranscription(audio_data);
            
            // Return the result
            json response = {
                {"text", transcription}
            };
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing transcription request: {}", e.what());
            res.status = 500;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });
}

std::string WhisperService::handleTranscription(const std::vector<uint8_t>& audio_data) {
    return whisper->audioToText(audio_data);
}

void WhisperService::start() {
    if (!running) {
        running = true;
        LOG_INFO("Starting Whisper service on {}:{}", host, port);
        if (!server->listen(host.c_str(), port)) {
            running = false;
            throw std::runtime_error("Failed to start Whisper service");
        }
    }
}

void WhisperService::stop() {
    if (running) {
        server->stop();
        running = false;
        LOG_INFO("Whisper service stopped");
    }
} 