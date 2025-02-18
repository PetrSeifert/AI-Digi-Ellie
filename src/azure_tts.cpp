#include "azure_tts.hpp"
#include "httplib_config.hpp"
#include <httplib.h>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>
#include "config.hpp"
#include "logging.hpp"
using json = nlohmann::json;

AzureTTS::AzureTTS(const std::string& subscription_key, const std::string& region)
    : subscription_key(subscription_key), region(region), token_expiry(0) {
    refreshToken();
}

AzureTTS::~AzureTTS() {}

void AzureTTS::refreshToken() {
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
    
    // Clean up the client
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

std::string AzureTTS::getAccessToken() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (now >= token_expiry) {
        refreshToken();
    }
    return access_token;
}

std::vector<uint8_t> AzureTTS::textToSpeech(const std::string& text, const std::string& voice) {
    // Get access token
    std::string token = getAccessToken();
    
    // Prepare SSML
    std::stringstream ssml;
    ssml << "<speak version='1.0' xml:lang='en-US'>"
         << "<voice xml:lang='en-US' xml:gender='Female' name='" << voice << "'>"
         << text
         << "</voice></speak>";

    std::string ssml_text = ssml.str();
    LOG_DEBUG("SSML Request:\n{}", ssml_text);

    std::string host = region + ".tts.speech.microsoft.com";
    LOG_INFO("Connecting to TTS service: {}", host);

    httplib::SSLClient* cli = new httplib::SSLClient(host.c_str());
    LOG_INFO("Using SSL for TTS connection");

    if (!cli) {
        throw std::runtime_error("Failed to create HTTP client");
    }

    cli->set_connection_timeout(10);
    
    // Set headers
    httplib::Headers headers = {
        {"X-Microsoft-OutputFormat", "raw-24khz-16bit-mono-pcm"},
        {"Content-Type", "application/ssml+xml"},
        {"Host", host},
        {"Content-Length", std::to_string(ssml_text.length())},
        {"Authorization", "Bearer " + token},
        {"User-Agent", config::AZURE_SPEECH_APP_NAME}
    };

    // Print request details for debugging
    LOG_DEBUG("Request Headers:");
    for (const auto& header : headers) {
        if (header.first != "Authorization") { // Don't print the auth token
            LOG_DEBUG("{}: {}", header.first, header.second);
        }
    }
    LOG_DEBUG("Request Body:\n{}", ssml_text);
    
    LOG_INFO("Sending TTS request...");
    auto result = cli->Post("/cognitiveservices/v1", headers, ssml_text, "application/ssml+xml");
    
    // Clean up the client
    delete cli;
    
    if (result) {
        LOG_INFO("Got TTS response with status: {}", result->status);
        LOG_DEBUG("Response Headers:");
        for (const auto& header : result->headers) {
            LOG_DEBUG("{}: {}", header.first, header.second);
        }
        
        if (result->status == 200) {
            const std::string& body = result->body;
            LOG_INFO("Received audio data of size: {} bytes", body.size());
            
            // Validate audio data size
            size_t expected_sample_size = 2; // 16-bit = 2 bytes per sample
            if (body.size() % expected_sample_size != 0) {
                throw std::runtime_error("Received malformed audio data: size is not aligned with 16-bit samples");
            }
            
            return std::vector<uint8_t>(body.begin(), body.end());
        } else {
            LOG_ERROR("Error Response Body: {}", result->body);
            throw std::runtime_error("Failed to convert text to speech: HTTP " + 
                std::to_string(result->status) + " - " + result->body);
        }
    } else {
        auto err = result.error();
        LOG_ERROR("TTS connection error code: {}", static_cast<int>(err));
        throw std::runtime_error("Failed to connect to Azure TTS: Connection error - " + std::string(httplib::to_string(err)));
    }
} 