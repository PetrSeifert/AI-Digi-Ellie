#include "azure_stt.hpp"
#include "httplib_config.hpp"
#include <httplib.h>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>
#include "config.hpp"
#include "logging.hpp"

using json = nlohmann::json;

AzureSTT::AzureSTT(const std::string& subscription_key, const std::string& region)
    : subscription_key(subscription_key), region(region), token_expiry(0) {
    refreshToken();
}

AzureSTT::~AzureSTT() {}

void AzureSTT::refreshToken() {
    std::string host = region + ".api.cognitive.microsoft.com";
    LOG_INFO("Connecting to: {}", host);

    httplib::SSLClient* cli = new httplib::SSLClient(host.c_str());
    LOG_INFO("Using SSL for connection");

    if (!cli) {
        throw std::runtime_error("Failed to create HTTP client");
    }

    cli->set_connection_timeout(10);
    
    httplib::Headers headers = {
        {"Ocp-Apim-Subscription-Key", subscription_key},
    };

    LOG_INFO("Requesting token...");
    auto result = cli->Post("/sts/v1.0/issueToken", headers, "", "");
    
    delete cli;
    
    if (result) {
        LOG_INFO("Got response with status: {}", result->status);
        if (result->status == 200) {
            access_token = result->body;
            // Token is valid for 10 minutes, we'll refresh after 9 minutes
            token_expiry = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now() + std::chrono::minutes(9)
            );
            LOG_INFO("Token refreshed successfully");
        } else {
            throw std::runtime_error("Failed to get Azure access token: HTTP " + 
                std::to_string(result->status) + " - " + result->body);
        }
    } else {
        auto err = result.error();
        LOG_ERROR("Connection error code: {}", static_cast<int>(err));
        throw std::runtime_error("Failed to connect to Azure: Connection error - " + std::string(httplib::to_string(err)));
    }
}

std::string AzureSTT::getAccessToken() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (now >= token_expiry) {
        refreshToken();
    }
    return access_token;
}

std::string AzureSTT::audioToText(const std::vector<uint8_t>& audio_data) {
    // Get access token
    std::string token = getAccessToken();
    
    std::string host = region + ".stt.speech.microsoft.com";
    LOG_INFO("Connecting to STT service: {}", host);

    httplib::SSLClient* cli = new httplib::SSLClient(host.c_str());
    LOG_INFO("Using SSL for STT connection");

    if (!cli) {
        throw std::runtime_error("Failed to create HTTP client");
    }

    cli->set_connection_timeout(10);
    
    // Set headers
    httplib::Headers headers = {
        {"Content-Type", "audio/wav; codecs=audio/pcm; samplerate=48000"},
        {"Authorization", "Bearer " + token},
        {"Accept", "application/json"}
    };

    // Print request details for debugging
    LOG_DEBUG("Request Headers:");
    for (const auto& header : headers) {
        if (header.first != "Authorization") { // Don't print the auth token
            LOG_DEBUG("{}: {}", header.first, header.second);
        }
    }
    
    LOG_INFO("Sending STT request with {} bytes of audio data...", audio_data.size());
    auto result = cli->Post("/speech/recognition/conversation/cognitiveservices/v1?language=en-US", 
                           headers, 
                           std::string(audio_data.begin(), audio_data.end()),
                           "audio/wav; codecs=audio/pcm; samplerate=48000");
    
    delete cli;
    
    if (result) {
        LOG_INFO("Got STT response with status: {}", result->status);
        LOG_DEBUG("Response Headers:");
        for (const auto& header : result->headers) {
            LOG_DEBUG("{}: {}", header.first, header.second);
        }
        
        if (result->status == 200) {
            try {
                json response = json::parse(result->body);
                if (response.contains("DisplayText")) {
                    std::string text = response["DisplayText"].get<std::string>();
                    LOG_INFO("Successfully transcribed audio to text: {}", text);
                    return text;
                } else {
                    LOG_ERROR("Response does not contain DisplayText field: {}", result->body);
                    throw std::runtime_error("Invalid response format from Azure STT");
                }
            } catch (const json::exception& e) {
                LOG_ERROR("Failed to parse JSON response: {}", e.what());
                throw std::runtime_error("Failed to parse Azure STT response");
            }
        } else {
            LOG_ERROR("Error Response Body: {}", result->body);
            throw std::runtime_error("Failed to convert audio to text: HTTP " + 
                std::to_string(result->status) + " - " + result->body);
        }
    } else {
        auto err = result.error();
        LOG_ERROR("STT connection error code: {}", static_cast<int>(err));
        throw std::runtime_error("Failed to connect to Azure STT: Connection error - " + std::string(httplib::to_string(err)));
    }
} 