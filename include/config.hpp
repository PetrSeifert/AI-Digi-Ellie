#pragma once
#include "logging.hpp"

#include <string>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <cctype>

namespace config {
    inline std::string getEnvVar(const char* name, const char* defaultValue = "") {
        const char* value = std::getenv(name);
        if (value == nullptr) {
            LOG_WARN("Environment variable {} not found. Using default value: {}", name, defaultValue);
            value = defaultValue;
        }
    
        return value;
    }

    inline uint64_t getEnvVarUInt64(const char* name, uint64_t defaultValue = 0) {
        const char* value = std::getenv(name);
        if (value == nullptr) {
            LOG_WARN("Environment variable {} not found. Using default value: {}", name, defaultValue);
            return defaultValue;
        }

        try {
            return std::stoull(value);
        } catch (...) {
            LOG_ERROR("Environment variable {} is not a valid unsigned long long. Using default value: {}", name, defaultValue);
            return defaultValue;
        }
    }

    const std::string DISCORD_TOKEN = getEnvVar("DIGI_ELLIE_DISCORD_TOKEN");

    // The name of the AI model to use with Ollama
    const std::string MODEL_NAME = getEnvVar("DIGI_ELLIE_MODEL_NAME", "mistral");
    
    // The host of the Ollama server
    const std::string LLM_HOST = getEnvVar("DIGI_ELLIE_OLLAMA_HOST", "localhost");

    // The Discord channel ID where the bot will send boot messages
    const uint64_t DEFAULT_CHANNEL_ID = getEnvVarUInt64("DIGI_ELLIE_DEFAULT_CHANNEL_ID", 0);

    // Azure TTS Configuration
    const std::string AZURE_SPEECH_APP_NAME = getEnvVar("DIGI_ELLIE_AZURE_SPEECH_APP_NAME");
    const std::string AZURE_SPEECH_KEY = getEnvVar("DIGI_ELLIE_AZURE_SPEECH_KEY");
    const std::string AZURE_SPEECH_REGION = getEnvVar("DIGI_ELLIE_AZURE_SPEECH_REGION", "germanywestcentral");
    const std::string AZURE_SPEECH_VOICE = getEnvVar("DIGI_ELLIE_AZURE_SPEECH_VOICE", "en-US-JennyNeural");

    // Whisper STT Configuration
    const std::string WHISPER_MODEL_NAME = getEnvVar("DIGI_ELLIE_WHISPER_MODEL_NAME", "ggml-large-v3-turbo-q8_0.bin");
    
    // Whisper Service Configuration
    const std::string WHISPER_SERVICE_HOST = getEnvVar("DIGI_ELLIE_WHISPER_SERVICE_HOST", "0.0.0.0");
    const uint64_t WHISPER_SERVICE_PORT = getEnvVarUInt64("DIGI_ELLIE_WHISPER_SERVICE_PORT", 8000);
} 