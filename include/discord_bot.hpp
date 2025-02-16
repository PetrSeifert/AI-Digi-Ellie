#pragma once

#include "discord_bot/core.hpp"
#include "discord_bot/voice.hpp"
#include "discord_bot/commands.hpp"
#include "discord_bot/tts.hpp"
#include "discord_bot/message.hpp"
#include <memory>

class DiscordBot {
public:
    explicit DiscordBot(const std::string& token);
    ~DiscordBot();

    void start();
    void stop();

private:
    void initializeModules();

    std::shared_ptr<discord::CoreBot> core;
    std::shared_ptr<discord::VoiceModule> voice;
    std::shared_ptr<discord::CommandsModule> commands;
    std::shared_ptr<discord::TTSModule> tts;
    std::shared_ptr<discord::MessageModule> message;
}; 