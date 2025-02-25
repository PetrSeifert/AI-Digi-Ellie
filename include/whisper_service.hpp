#pragma once

#include "whisper_stt.hpp"
#include "httplib.h"
#include <string>
#include <memory>
#include <vector>

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
}; 