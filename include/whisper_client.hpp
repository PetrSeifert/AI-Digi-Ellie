#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "httplib.h"

class WhisperClient {
public:
    WhisperClient(const std::string& service_url, int retry_delay_ms = 2000);
    ~WhisperClient();

    // Convert audio data to text using the remote service
    std::string audioToText(const std::vector<uint8_t>& audio_data);

    // Check if the service is available
    bool isHealthy();
    
    // Try to reconnect to the service (non-blocking)
    void startReconnectionTask();
    
    // Stop the reconnection task
    void stopReconnectionTask();

private:
    std::string service_url;
    std::unique_ptr<httplib::Client> client;
    int retry_delay_ms;
    
    std::atomic<bool> is_reconnecting;
    std::atomic<bool> should_stop_reconnection;
    std::unique_ptr<std::thread> reconnection_thread;
    std::mutex client_mutex;
    
    void initClient();
    void reconnectionLoop();
}; 