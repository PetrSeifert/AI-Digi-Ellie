#include "discord_bot.hpp"
#include "conversation.hpp"
#include "inference.hpp"
#include "config.hpp"
#include "logging.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

DiscordBot::DiscordBot(const std::string& token) : is_running(false), voice_connected(false), current_voice_channel(0) {
    LOG_INFO("Creating Discord bot...");
    
    // Set up intents to receive message content and other required intents
    uint32_t intents = dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_messages | dpp::i_direct_messages;
    bot = std::make_unique<dpp::cluster>(token, intents);
    
    // Initialize Azure TTS if key is provided
    if (!config::AZURE_TTS_KEY.empty()) {
        tts = std::make_unique<AzureTTS>(config::AZURE_TTS_KEY, config::AZURE_TTS_REGION);
    }
    
    LOG_INFO("Bot instance created with intents: 0x{:x}", intents);
    LOG_INFO("Make sure Message Content Intent is enabled in Discord Developer Portal!");
    
    LOG_INFO("Setting up events...");
    setupEvents();
    LOG_INFO("Events setup completed.");
}

DiscordBot::~DiscordBot() {
    stop();
}

void DiscordBot::registerCommands() {
    if (!bot) return;

    LOG_INFO("Registering commands for application ID: {}", bot->me.id);

    std::vector<dpp::slashcommand> commands;

    // Create the clear command
    dpp::slashcommand clearCommand("clear", "Clear the conversation history", bot->me.id);
    clearCommand.default_member_permissions = 0;
    commands.push_back(clearCommand);
    
    // Create the shutdown command
    dpp::slashcommand shutdownCommand("shutdown", "Shutdown the bot", bot->me.id);
    shutdownCommand.default_member_permissions = 0;
    commands.push_back(shutdownCommand);

    // Create the join voice command
    dpp::slashcommand joinVoiceCommand("joinvoice", "Join your current voice channel", bot->me.id);
    joinVoiceCommand.default_member_permissions = 0;
    commands.push_back(joinVoiceCommand);

    size_t commandCount = commands.size();

    // Bulk register all commands globally
    bot->global_bulk_command_create(commands, [this, commandCount](const dpp::confirmation_callback_t& callback) {
        if (callback.is_error()) {
            LOG_ERROR("Error registering commands: {}", callback.get_error().message);
        } else {
            LOG_INFO("Successfully registered {} slash commands.", commandCount);
        }
    });
}

void DiscordBot::setupEvents() {
    if (!bot) {
        LOG_ERROR("Error: Bot instance is null during event setup!");
        return;
    }

    LOG_INFO("Setting up on_ready event...");

    // Log when bot is ready
    bot->on_ready([this](const dpp::ready_t& event) {
        LOG_INFO("=== Bot Ready Event ===");
        LOG_INFO("Logged in as: {}", bot->me.username);
        LOG_INFO("Bot ID: {}", bot->me.id);
        LOG_INFO("=====================");

        // Register commands after bot is ready
        registerCommands();

        // Send boot message to Ellie
        std::string bootPrompt = buildPrompt("<Bootup>", "System");
        std::string bootResponse = runInference(bootPrompt);

        // Send to the default channel
        bot->message_create(dpp::message(dpp::snowflake(config::DEFAULT_CHANNEL_ID), bootResponse));
    });

    // Handle slash commands
    bot->on_slashcommand([this](const dpp::slashcommand_t& event) {
        handleSlashCommand(event);
    });

    LOG_INFO("Setting up on_message_create event...");

    // Handle incoming messages
    bot->on_message_create([this](const dpp::message_create_t& event) {
        handleMessage(event);
    });

    // Add log handler
    bot->on_log([](const dpp::log_t& event) {
        std::string level = "INFO";
        if (event.severity == dpp::ll_critical) {
            level = "CRITICAL";
        } else if (event.severity == dpp::ll_error) {
            level = "ERROR";
        } else if (event.severity == dpp::ll_warning) {
            level = "WARNING";
        }
        LOG_INFO("{}: {}", level, event.message);
    });
}

void DiscordBot::handleSlashCommand(const dpp::slashcommand_t& event) {
    const std::string& commandName = event.command.get_command_name();
    
    if (commandName == "clear") {
        clearCommand(event);
    }
    else if (commandName == "shutdown") {
        shutdownCommand(event);
    }
    else if (commandName == "joinvoice") {
        joinVoiceCommand(event);
    }
}

void DiscordBot::clearCommand(const dpp::slashcommand_t& event) {
    clearHistory();
    event.reply("Conversation history cleared.");
}

void DiscordBot::shutdownCommand(const dpp::slashcommand_t& event) {
    event.reply("Shuting down...");
    // Send shutdown message to Ellie
    std::string shutdownPrompt = buildPrompt("<Shutdown>", "System");
    std::string shutdownResponse = runInference(shutdownPrompt);
    
    event.edit_response(shutdownResponse, [this](const dpp::confirmation_callback_t& callback) {
        stop();
    });
}

void DiscordBot::joinVoiceCommand(const dpp::slashcommand_t& event) {
    // Get the guild member who issued the command
    auto guild_id = event.command.guild_id;
    
    dpp::guild* g = dpp::find_guild(guild_id);
    if (!g) {
        event.reply("Failed to find the guild!");
        return;
    }

    if (!g->connect_member_voice(event.command.get_issuing_user().id)) {
        event.reply("You need to be in a voice channel first!");
        return;
    }

    voice_connected = true;
    current_voice_channel = event.command.channel_id;
    event.reply("Joined your voice channel!");
}

void DiscordBot::handleMessage(const dpp::message_create_t& event) {
    // Ignore messages from bots (including ourselves)
    if (event.msg.author.is_bot()) {
        return;
    }

    // Get the user's name and message
    std::string userName = event.msg.author.username;
    std::string userMessage = event.msg.content;

    // Debug output
    LOG_INFO("=== Incoming Discord Message ===");
    LOG_INFO("From: {} (ID: {})", userName, event.msg.author.id);
    LOG_INFO("Channel: {}", event.msg.channel_id);
    LOG_INFO("Content: {}", userMessage);
    LOG_INFO("Message Length: {}", userMessage.length());
    LOG_DEBUG("Raw Content Bytes: {}", [&userMessage]() {
        std::stringstream ss;
        for (unsigned char c : userMessage) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
        }
        return ss.str();
    }());
    LOG_INFO("==============================");

    // Build the prompt from the user's message
    std::string prompt = buildPrompt(userMessage, userName);
    
    // Debug output for prompt
    LOG_DEBUG("=== Generated Prompt ===");
    LOG_DEBUG("{}", prompt);
    LOG_DEBUG("=====================");

    // Get Ellie's response
    std::string ellieResponse = runInference(prompt);

    // Debug output for response
    LOG_INFO("=== Ellie's Response ===");
    LOG_INFO("{}", ellieResponse);
    LOG_INFO("==================");

    // Send the response back to Discord
    if (!ellieResponse.empty()) {
        // If we're in a voice channel and TTS is enabled, speak the response
        if (voice_connected && tts && current_voice_channel) {
            speakText(ellieResponse, event.msg.guild_id);
        }

        // Split long messages if they exceed Discord's limit
        const size_t MAX_MESSAGE_LENGTH = 2000;
        if (ellieResponse.length() > MAX_MESSAGE_LENGTH) {
            LOG_INFO("Response exceeds Discord limit, splitting into chunks...");
            for (size_t i = 0; i < ellieResponse.length(); i += MAX_MESSAGE_LENGTH) {
                std::string chunk = ellieResponse.substr(i, MAX_MESSAGE_LENGTH);
                bot->message_create(dpp::message(event.msg.channel_id, chunk));
                LOG_DEBUG("Sent chunk of size: {}", chunk.length());
            }
        } else {
            bot->message_create(dpp::message(event.msg.channel_id, ellieResponse));
        }
    }
}

void DiscordBot::speakText(const std::string& text, dpp::snowflake guild_id) {
    if (!tts) {
        LOG_WARN("TTS not initialized - missing Azure key");
        return;
    }

    try {
        // Convert text to speech
        std::vector<uint8_t> audio_data = tts->textToSpeech(text, config::AZURE_TTS_VOICE);
        
        // Get voice connection for the guild
        if (auto vconn = bot->get_shard(0)->get_voice(guild_id)) {
            // Convert audio format and send
            std::vector<uint16_t> stereo_data = convertTTSAudioFormat(audio_data);
            vconn->voiceclient->send_audio_raw(stereo_data.data(), stereo_data.size() * sizeof(uint16_t));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error in TTS: {}", e.what());
    }
}

std::vector<uint16_t> DiscordBot::convertTTSAudioFormat(const std::vector<uint8_t>& mono_24khz) {
    // Convert from 24kHz mono to 48kHz stereo
    std::vector<uint16_t> stereo_data;
    const int16_t* mono_samples = reinterpret_cast<const int16_t*>(mono_24khz.data());
    size_t num_samples = mono_24khz.size() / sizeof(int16_t);
    
    // Reserve space for stereo 48kHz data (2 channels, 2x sample rate)
    stereo_data.reserve(num_samples * 4);
    
    // Simple linear interpolation for upsampling
    for (size_t i = 0; i < num_samples - 1; i++) {
        // Get two adjacent samples for interpolation
        int16_t current = mono_samples[i];
        int16_t next = mono_samples[i + 1];
        
        // Add first sample (duplicate for stereo)
        stereo_data.push_back(static_cast<uint16_t>(current));
        stereo_data.push_back(static_cast<uint16_t>(current));
        
        // Add interpolated sample (duplicate for stereo)
        int16_t interpolated = (current + next) / 2;
        stereo_data.push_back(static_cast<uint16_t>(interpolated));
        stereo_data.push_back(static_cast<uint16_t>(interpolated));
    }
    
    // Add the last sample
    stereo_data.push_back(static_cast<uint16_t>(mono_samples[num_samples - 1]));
    stereo_data.push_back(static_cast<uint16_t>(mono_samples[num_samples - 1]));
    
    LOG_DEBUG("Converted {} mono samples to stereo", num_samples);
    
    return stereo_data;
}

void DiscordBot::start() {
    if (!is_running) {
        LOG_INFO("Starting Discord bot...");
        is_running = true;
        try {
            bot->start(dpp::st_wait);  // Start and wait for events
        } catch (const std::exception& e) {
            LOG_ERROR("Error starting bot: {}", e.what());
            is_running = false;
        }
    }
}

void DiscordBot::stop() {
    if (is_running) {
        LOG_INFO("Stopping Discord bot...");
        is_running = false;
        bot->shutdown();
        LOG_INFO("Bot stopped.");
    }
} 