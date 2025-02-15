#pragma once
#include <dpp/dpp.h>
#include <string>
#include <memory>
#include "config.hpp"

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
    void handleSlashCommand(const dpp::slashcommand_t& event);
    void clearCommand(const dpp::slashcommand_t& event);
    void shutdownCommand(const dpp::slashcommand_t& event);
    
    std::unique_ptr<dpp::cluster> bot;
    bool is_running;
}; 