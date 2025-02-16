#pragma once
#include <dpp/dpp.h>
#include <string>
#include <memory>
#include <unordered_map>
#include "config.hpp"
#include "azure_tts.hpp"

class DiscordBot {
public:
    DiscordBot(const std::string& token);
    ~DiscordBot();

    void start();
    void stop();
    bool isRunning() const { return is_running; }

private:
    bool is_running;
    std::unique_ptr<dpp::cluster> bot;
    std::unique_ptr<AzureTTS> tts;
    bool voice_connected;

    std::unordered_map<dpp::snowflake, std::vector<uint8_t>> recording_buffers;
    bool is_recording;

    void setupEvents();
    void handleMessage(const dpp::message_create_t& event);
    void registerCommands();
    void handleSlashCommand(const dpp::slashcommand_t& event);
    void clearCommand(const dpp::slashcommand_t& event);
    void shutdownCommand(const dpp::slashcommand_t& event);
    void joinVoiceCommand(const dpp::slashcommand_t& event);
    void recordCommand(const dpp::slashcommand_t& event);
    void stopRecordCommand(const dpp::slashcommand_t& event);
    
    void speakText(const std::string& text, dpp::snowflake channel_id);
    
    std::vector<uint16_t> convertTTSAudioFormat(const std::vector<uint8_t>& mono_24khz);
}; 