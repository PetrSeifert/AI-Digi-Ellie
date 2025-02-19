#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

using STTCallback = std::function<void(const std::string&)>;

class AzureSTT {
public:
    AzureSTT(const std::string& subscription_key, const std::string& region);
    ~AzureSTT();

    std::string audioToText(const std::vector<uint8_t>& audio_data);

    // New streaming methods
    void startStreaming(STTCallback callback);
    void streamAudioChunk(const uint8_t* audio, size_t audio_size);
    void stopStreaming();

private:
    std::string subscription_key;
    std::string region;
    std::string access_token;
    int64_t token_expiry;
    
    // Streaming state
    bool is_streaming;
    STTCallback stream_callback;
    std::vector<uint8_t> stream_buffer;
    static constexpr size_t STREAM_BUFFER_THRESHOLD = 8192; // Process every ~8KB of audio

    void refreshToken();
    std::string getAccessToken();
    void processStreamBuffer();
}; 