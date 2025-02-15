#pragma once
#include <string>
#include <cstdint>

namespace config {
    // The name of the AI model to use with Ollama
    const std::string MODEL_NAME = "mistral";

    // The Discord channel ID where the bot will send boot messages
    const uint64_t DEFAULT_CHANNEL_ID = 829618675475939392;
} 