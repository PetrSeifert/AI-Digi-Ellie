#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

class AzureTTS {
public:
    AzureTTS(const std::string& subscription_key, const std::string& region);
    ~AzureTTS();

    // Convert text to speech and return raw PCM audio data (24kHz, 16-bit, mono)
    std::vector<uint8_t> textToSpeech(const std::string& text, const std::string& voice);

private:
    std::string subscription_key;
    std::string region;
    std::string access_token;
    int64_t token_expiry;

    void refreshToken();
    std::string getAccessToken();
}; 