#pragma once
#include <dpp/dpp.h>
#include <string>
#include <memory>

class DiscordBot {
public:
    DiscordBot(const std::string& token);
    ~DiscordBot();

    void start();
    void stop();
    bool isRunning() const { return is_running; }

private:
    void setupEvents();
    void handleMessage(const dpp::message_create_t& event);
    void registerCommands();
    
    std::unique_ptr<dpp::cluster> bot;
    bool is_running;

    const uint64_t DEFAULT_CHANNEL_ID = 829618675475939392;
}; 