#include "discord_bot.hpp"
#include "conversation.hpp"
#include "inference.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

DiscordBot::DiscordBot(const std::string& token) : is_running(false) {
    std::cout << "Creating Discord bot...\n";
    
    // Set up intents to receive message content and other required intents
    uint32_t intents = dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_messages | dpp::i_direct_messages;
    bot = std::make_unique<dpp::cluster>(token, intents);
    
    std::cout << "Bot instance created with intents: 0x" << std::hex << intents << std::dec << "\n";
    std::cout << "Make sure Message Content Intent is enabled in Discord Developer Portal!\n";
    
    // Log startup
    std::cout << "Setting up events...\n";
    setupEvents();
    registerCommands();
    std::cout << "Events setup completed.\n";
}

DiscordBot::~DiscordBot() {
    stop();
}

void DiscordBot::registerCommands() {
    if (!bot) return;
}

void DiscordBot::setupEvents() {
    if (!bot) {
        std::cerr << "Error: Bot instance is null during event setup!\n";
        return;
    }

    std::cout << "Setting up on_ready event...\n";

    // Log when bot is ready
    bot->on_ready([this](const dpp::ready_t& event) {
        std::cout << "\n=== Bot Ready Event ===\n";
        std::cout << "Logged in as: " << bot->me.username << "\n";
        std::cout << "Bot ID: " << bot->me.id << "\n";
        std::cout << "=====================\n";

        // Send boot message to Ellie
        std::string bootPrompt = buildPrompt("<Bootup>", "System");
        std::string bootResponse = runInference(bootPrompt);

        // Send to the default channel
        bot->message_create(dpp::message(dpp::snowflake(DEFAULT_CHANNEL_ID), bootResponse));
    });

    std::cout << "Setting up on_message_create event...\n";

    // Handle incoming messages
    bot->on_message_create([this](const dpp::message_create_t& event) {
        handleMessage(event);
    });

    // Add log handler
    bot->on_log([](const dpp::log_t& event) {
        switch (event.severity) {
            case dpp::ll_critical:
                std::cout << "CRITICAL: ";
                break;
            case dpp::ll_error:
                std::cout << "ERROR: ";
                break;
            case dpp::ll_warning:
                std::cout << "WARNING: ";
                break;
            case dpp::ll_info:
                std::cout << "INFO: ";
                break;
            case dpp::ll_debug:
                std::cout << "DEBUG: ";
                break;
        }
        std::cout << event.message << std::endl;
    });
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
    std::cout << "\n=== Incoming Discord Message ===\n";
    std::cout << "From: " << userName << " (ID: " << event.msg.author.id << ")\n";
    std::cout << "Channel: " << event.msg.channel_id << "\n";
    std::cout << "Content: " << userMessage << "\n";
    std::cout << "Message Length: " << userMessage.length() << "\n";
    std::cout << "Raw Content Bytes: ";
    for (unsigned char c : userMessage) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << "\n";
    std::cout << "==============================\n";

    // Special commands
    // TODO: Rework into slash commands
    if (userMessage == "/clear") {
        clearHistory();
        bot->message_create(dpp::message(event.msg.channel_id, "Conversation history cleared."));
        return;
    }
    else if (userMessage == "/shutdown") {
        // Send shutdown message to Ellie
        std::string shutdownPrompt = buildPrompt("<Shutdown>", "System");
        std::string shutdownResponse = runInference(shutdownPrompt);
        bot->message_create(dpp::message(event.msg.channel_id, shutdownResponse), [this](const dpp::confirmation_callback_t& callback) {
            stop();
        });

        return;
    }

    // Build the prompt from the user's message
    std::string prompt = buildPrompt(userMessage, userName);
    
    // Debug output for prompt
    std::cout << "\n=== Generated Prompt ===\n";
    std::cout << prompt << "\n";
    std::cout << "=====================\n";

    // Get Ellie's response
    std::string ellieResponse = runInference(prompt);

    // Debug output for response
    std::cout << "\n=== Ellie's Response ===\n";
    std::cout << ellieResponse << "\n";
    std::cout << "==================\n";

    // Send the response back to Discord
    if (!ellieResponse.empty()) {
        // Split long messages if they exceed Discord's limit
        const size_t MAX_MESSAGE_LENGTH = 2000;
        if (ellieResponse.length() > MAX_MESSAGE_LENGTH) {
            std::cout << "Response exceeds Discord limit, splitting into chunks...\n";
            for (size_t i = 0; i < ellieResponse.length(); i += MAX_MESSAGE_LENGTH) {
                std::string chunk = ellieResponse.substr(i, MAX_MESSAGE_LENGTH);
                bot->message_create(dpp::message(event.msg.channel_id, chunk));
                std::cout << "Sent chunk of size: " << chunk.length() << "\n";
            }
        } else {
            bot->message_create(dpp::message(event.msg.channel_id, ellieResponse));
        }
    }
}

void DiscordBot::start() {
    if (!is_running) {
        std::cout << "Starting Discord bot...\n";
        is_running = true;
        try {
            bot->start(dpp::st_wait);  // Start and wait for events
        } catch (const std::exception& e) {
            std::cerr << "Error starting bot: " << e.what() << "\n";
            is_running = false;
        }
    }
}

void DiscordBot::stop() {
    if (is_running) {
        std::cout << "Stopping Discord bot...\n";
        is_running = false;
        bot->shutdown();
        std::cout << "Bot stopped.\n";
    }
} 