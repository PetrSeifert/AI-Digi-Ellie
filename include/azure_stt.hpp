#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

class AzureSTT {
public:
    AzureSTT(const std::string& subscription_key, const std::string& region);
    ~AzureSTT();

    std::string audioToText(const std::vector<uint8_t>& audio_data);

private:
    std::string subscription_key;
    std::string region;
    std::string access_token;
    int64_t token_expiry;

    void refreshToken();
    std::string getAccessToken();
}; 