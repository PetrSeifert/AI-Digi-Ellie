#pragma once

#include "whisper_stt.hpp"
#include "httplib.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>

class WhisperService {
public:
    WhisperService(const std::string& model_path, const std::string& host, int port);
    ~WhisperService();

    void start();
    void stop();

private:
    std::string host;
    int port;
    std::unique_ptr<WhisperSTT> whisper;
    std::unique_ptr<httplib::Server> server;
    bool running;

    void setupRoutes();
    std::string handleTranscription(const std::vector<uint8_t>& audio_data);

    // In-memory stream sessions: store accumulated raw bytes per session
    std::mutex sessions_mutex;
    std::unordered_map<std::string, std::vector<uint8_t>> sessions;

    // Serialize whisper usage across requests for safety
    std::mutex whisper_mutex;
}; 