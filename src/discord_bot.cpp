#include "discord_bot.hpp"
#include "config.hpp"
#include "logging.hpp"

DiscordBot::DiscordBot(const std::string& token) {
    LOG_INFO("Creating Discord bot...");
    
    // Create core bot module only
    core = std::make_shared<discord::CoreBot>(token);
    
    // Register ready callback to initialize other modules
    core->onReady([this]() {
        initializeModules();
    });
    
    LOG_INFO("Core bot module initialized. Waiting for ready event...");
}

void DiscordBot::initializeModules() {
    LOG_INFO("Initializing bot modules...");
    
    // Create commands module first since other modules need it
    commands = std::make_shared<discord::CommandsModule>(core);
    
    // Create voice module
    voice = std::make_shared<discord::VoiceModule>(core, commands);
    
    // Create TTS module if Azure key is provided
    if (!config::AZURE_TTS_KEY.empty()) {
        tts = std::make_shared<discord::TTSModule>(core, config::AZURE_TTS_KEY, config::AZURE_TTS_REGION);
    }
    
    // Create message module
    message = std::make_shared<discord::MessageModule>(core);
    
    // Wire up event connections
    if (tts && voice) {
        message->onResponse([this](const std::string& response, dpp::snowflake guild_id) {
            if (tts->isEnabled() && voice->isVoiceConnected()) {
                tts->speakText(response, guild_id);
            }
        });
    }
    
    // Register all commands after all modules are initialized
    commands->registerCommands();
    
    LOG_INFO("Bot modules initialized successfully.");
}

DiscordBot::~DiscordBot() {
    stop();
}

void DiscordBot::start() {
    core->start();
}

void DiscordBot::stop() {
    core->stop();
}