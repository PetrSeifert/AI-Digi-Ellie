#include "discord_bot.hpp"
#include "conversation.hpp"
#include "inference.hpp"
#include "config.hpp"
#include "logging.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

DiscordBot::DiscordBot(const std::string& token) : is_running(false), voice_connected(false),
    is_recording(false) {
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

    // Create the record command
    dpp::slashcommand recordCommand("record", "Start recording a user's voice", bot->me.id);
    recordCommand.default_member_permissions = 0;
    commands.push_back(recordCommand);

    // Create the stop recording command
    dpp::slashcommand stopRecordCommand("stoprecord", "Stop recording voice", bot->me.id);
    stopRecordCommand.default_member_permissions = 0;
    commands.push_back(stopRecordCommand);

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
        if (event.severity == dpp::ll_critical) {
            LOG_CRITICAL(event.message);
        } else if (event.severity == dpp::ll_error) {
            LOG_ERROR(event.message);
        } else if (event.severity == dpp::ll_warning) {
            LOG_WARN(event.message);
        } else if (event.severity == dpp::ll_debug) {
            LOG_DEBUG(event.message);
        } else {
            LOG_INFO(event.message);
        }
    });

    // Add voice receive handler
    bot->on_voice_receive([this](const dpp::voice_receive_t& event) {
        if (is_recording) {
            auto& buffer = recording_buffers[event.user_id];
            size_t current_size = buffer.size();
            buffer.resize(current_size + event.audio_size);
            std::memcpy(buffer.data() + current_size, event.audio, event.audio_size);
            
            LOG_DEBUG("Recorded {} bytes of audio from user {} (total: {} bytes)", 
                     event.audio_size, event.user_id, buffer.size());
        }
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
    else if (commandName == "record") {
        recordCommand(event);
    }
    else if (commandName == "stoprecord") {
        stopRecordCommand(event);
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
        if (voice_connected && tts) {
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

void DiscordBot::recordCommand(const dpp::slashcommand_t& event) {
    // Check if we're already recording
    if (is_recording) {
        event.reply("Already recording! Stop the current recording first.");
        return;
    }

    // Get the guild
    dpp::guild* g = dpp::find_guild(event.command.guild_id);
    if (!g) {
        event.reply("Failed to find the guild!");
        return;
    }

    // Check if we're in a voice channel
    if (!voice_connected) {
        event.reply("Bot needs to be in a voice channel first! Use /joinvoice");
        return;
    }

    // Clear all recording buffers and initialize for all users in the voice channel
    recording_buffers.clear();
    
    // Get all users in the voice channel
    std::vector<dpp::snowflake> users_in_channel;
    for (const auto& [user_id, state] : g->voice_members) {
        recording_buffers[user_id].reserve(1024 * 1024); // Reserve 1MB initially for each user
        users_in_channel.push_back(user_id);
    }

    if (users_in_channel.empty()) {
        event.reply("No users found in the voice channel!");
        return;
    }

    is_recording = true;
    
    std::string user_list;
    for (size_t i = 0; i < users_in_channel.size(); ++i) {
        if (i > 0) user_list += ", ";
        user_list += "<@" + std::to_string(users_in_channel[i]) + ">";
    }

    event.reply("Started recording users: " + user_list);
}

void DiscordBot::stopRecordCommand(const dpp::slashcommand_t& event) {
    if (!is_recording) {
        event.reply("Not currently recording!");
        return;
    }

    is_recording = false;
    
    std::stringstream ss;
    ss << "Stopped recording. Results:\n";
    event.reply(ss.str() + "...");
    
    // Process each user's recording
    for (const auto& [user_id, buffer] : recording_buffers) {
        size_t recorded_bytes = buffer.size();
        float recorded_seconds = static_cast<float>(recorded_bytes) / (48000.0f * 2.0f * 2.0f); // 48kHz, 16-bit, stereo
        int recorded_milliseconds = static_cast<int>(recorded_seconds * 1000.0f);
        
        ss << "<@" << user_id << ">: " << recorded_seconds << " seconds (" << recorded_bytes << " bytes)\n";
        
        // Playback this user's audio
        if (auto vconn = bot->get_shard(0)->get_voice(event.command.guild_id)) {
            uint16_t* audio_data = reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(buffer.data()));
            size_t sample_count = buffer.size() / sizeof(uint16_t);
            
            if (sample_count > 0) {
                LOG_INFO("Playing back {} samples of audio from user {}", sample_count, user_id);
                vconn->voiceclient->send_audio_raw(audio_data, buffer.size());
            }
            
            // Wait for the audio to play back
            std::this_thread::sleep_for(std::chrono::milliseconds(recorded_milliseconds + 2000));
        }
    }
    
    event.edit_response(ss.str());
    
    // Clear all buffers
    recording_buffers.clear();
}