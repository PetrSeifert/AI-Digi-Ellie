#include "whisper_client.hpp"
#include "logging.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

WhisperClient::WhisperClient(const std::string& service_url, int retry_delay_ms) 
    : service_url(service_url), retry_delay_ms(retry_delay_ms),
      is_reconnecting(false), should_stop_reconnection(false) {
    
    initClient();
    
    // Try to connect initially
    if (!isHealthy()) {
        LOG_WARN("Initial connection to Whisper service failed, starting background reconnection task");
        startReconnectionTask();
    }
}

WhisperClient::~WhisperClient() {
    stopReconnectionTask();
}

void WhisperClient::initClient() {
    std::lock_guard<std::mutex> lock(client_mutex);
    client = std::make_unique<httplib::Client>(service_url);
    client->set_connection_timeout(5);
    client->set_read_timeout(30);
    
    LOG_INFO("Initialized Whisper client with service URL: {}", service_url);
}

void WhisperClient::startReconnectionTask() {
    // Don't start if already reconnecting
    if (is_reconnecting.exchange(true)) {
        return;
    }
    
    // Stop any existing thread
    stopReconnectionTask();
    
    // Reset stop flag
    should_stop_reconnection = false;
    
    // Start new reconnection thread
    reconnection_thread = std::make_unique<std::thread>(&WhisperClient::reconnectionLoop, this);
    LOG_INFO("Started background reconnection task for Whisper service");
}

void WhisperClient::stopReconnectionTask() {
    should_stop_reconnection = true;
    
    if (reconnection_thread && reconnection_thread->joinable()) {
        reconnection_thread->join();
        reconnection_thread.reset();
        LOG_INFO("Stopped background reconnection task for Whisper service");
    }
}

void WhisperClient::reconnectionLoop() {
    int attempt = 1;
    
    while (!should_stop_reconnection) {
        LOG_INFO("Reconnection attempt {} to Whisper service at {}", attempt++, service_url);
        
        // Re-initialize the client
        initClient();
        
        if (isHealthy()) {
            LOG_INFO("Successfully reconnected to Whisper service");
            is_reconnecting = false;
            return;  // Exit the thread on success
        }
        
        // Wait before next attempt
        if (!should_stop_reconnection) {
            LOG_INFO("Waiting {}ms before next reconnection attempt", retry_delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
        }
    }
    
    is_reconnecting = false;
}

std::string WhisperClient::audioToText(const std::vector<uint8_t>& audio_data) {
    if (audio_data.empty()) {
        LOG_WARN("Empty audio data provided to Whisper client");
        return "";
    }

    // Check if service is healthy, start reconnection if not
    if (!isHealthy()) {
        if (!is_reconnecting) {
            startReconnectionTask();
        }
        throw std::runtime_error("Whisper service is unavailable, reconnection task started");
    }

    // Set up headers
    httplib::Headers headers = {
        {"Content-Type", "audio/raw"}
    };

    // Send request
    LOG_INFO("Sending {} bytes of audio data to Whisper service", audio_data.size());
    
    std::string transcribed_text;
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        auto result = client->Post("/transcribe", headers, 
            std::string(audio_data.begin(), audio_data.end()), "audio/raw");

        if (result) {
            if (result->status == 200) {
                try {
                    auto response = json::parse(result->body);
                    transcribed_text = response["text"].get<std::string>();
                    LOG_INFO("Received transcription: {}", transcribed_text);
                } catch (const json::exception& e) {
                    throw std::runtime_error("Failed to parse Whisper service response: " + std::string(e.what()));
                }
            } else {
                try {
                    auto error = json::parse(result->body);
                    throw std::runtime_error("Whisper service error: " + error["error"].get<std::string>());
                } catch (const json::exception&) {
                    throw std::runtime_error("Whisper service error: HTTP " + std::to_string(result->status));
                }
            }
        } else {
            auto err = result.error();
            LOG_ERROR("Connection error to Whisper service: {}", httplib::to_string(err));
            
            // Start reconnection task in background
            if (!is_reconnecting) {
                startReconnectionTask();
            }
            
            throw std::runtime_error("Failed to connect to Whisper service: " + httplib::to_string(err));
        }
    }
    
    return transcribed_text;
}

std::string WhisperClient::startStream() {
    if (!isHealthy()) {
        if (!is_reconnecting) {
            startReconnectionTask();
        }
        throw std::runtime_error("Whisper service is unavailable, reconnection task started");
    }

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

    json payload = {};

    std::lock_guard<std::mutex> lock(client_mutex);
    auto result = client->Post("/stream/start", headers, payload.dump(), "application/json");
    if (!result) {
        auto err = result.error();
        throw std::runtime_error("Failed to start stream: " + httplib::to_string(err));
    }
    if (result->status != 200) {
        auto response = json::parse(result->body);
        throw std::runtime_error("Start stream error: " + response.value("error", std::string("HTTP ") + std::to_string(result->status)));
    }
    auto response = json::parse(result->body);
    return response.value("session_id", "");
}

std::string WhisperClient::appendStream(const std::string& session_id, const std::vector<uint8_t>& audio_chunk) {
    if (session_id.empty() || audio_chunk.empty()) return "";
    if (!isHealthy()) return "";

    httplib::Headers headers = {
        {"X-Session-Id", session_id},
        {"Content-Type", "audio/raw"}
    };

    std::lock_guard<std::mutex> lock(client_mutex);
    auto result = client->Post("/stream/chunk", headers, std::string(audio_chunk.begin(), audio_chunk.end()), "audio/raw");
    if (!result || result->status != 200) return "";
    try {
        auto response = json::parse(result->body);
        return response.value("partial", "");
    } catch (...) {
        return "";
    }
}

std::string WhisperClient::finishStream(const std::string& session_id) {
    if (session_id.empty()) return "";
    if (!isHealthy()) return "";

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };
    json payload = { {"session_id", session_id} };

    std::lock_guard<std::mutex> lock(client_mutex);
    auto result = client->Post("/stream/finish", headers, payload.dump(), "application/json");
    if (!result || result->status != 200) return "";
    auto response = json::parse(result->body);
    return response.value("text", "");
}

bool WhisperClient::isHealthy() {
    std::lock_guard<std::mutex> lock(client_mutex);
    auto result = client->Get("/health");
    return result && result->status == 200 && result->body == "OK";
} 