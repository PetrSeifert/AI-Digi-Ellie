#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <speechapi_cxx.h>

class AzureSTT {
public:
    AzureSTT(const std::string& subscription_key, const std::string& region);
    ~AzureSTT();

    std::string audioToText(const std::vector<uint8_t>& audio_data);

private:
    std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechConfig> speech_config;
    std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechRecognizer> recognizer;
}; 