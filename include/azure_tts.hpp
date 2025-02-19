#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <speechapi_cxx.h>

class AzureTTS {
public:
    AzureTTS(const std::string& subscription_key, const std::string& region);
    ~AzureTTS();

    // Convert text to speech and return raw PCM audio data (24kHz, 16-bit, mono)
    std::vector<uint8_t> textToSpeech(const std::string& text, const std::string& voice);

private:
    std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechConfig> speech_config;
    std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechSynthesizer> synthesizer;
}; 