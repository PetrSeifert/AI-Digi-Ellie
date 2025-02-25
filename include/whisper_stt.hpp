#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "whisper.h"

class WhisperSTT {
public:
    WhisperSTT(const std::string& model_path);
    ~WhisperSTT();

    // Convert audio data to text
    // Input: Raw PCM audio data (16kHz, 16-bit, mono)
    std::string audioToText(const std::vector<uint8_t>& audio_data);

private:
    struct whisper_context* ctx;
    struct whisper_state* state;
    
    // Convert raw PCM bytes to float samples
    std::vector<float> convertPCMToFloat(const std::vector<uint8_t>& audio_data);
}; 