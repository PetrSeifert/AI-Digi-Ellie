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
    std::cout << "Events setup completed.\n";
}

DiscordBot::~DiscordBot() {
    stop();
}

void DiscordBot::registerCommands() {
    if (!bot) return;

    std::cout << "Registering commands for application ID: " << bot->me.id << "\n";

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
            std::cout << "Error registering commands: " << callback.get_error().message << "\n";
        } else {
            std::cout << "Successfully registered " << commandCount << " slash commands.\n";
        }
    });
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

    std::cout << "Setting up on_message_create event...\n";

    // Handle incoming messages
    bot->on_message_create([this](const dpp::message_create_t& event) {
        handleMessage(event);
    });

    // Add log handler
    bot->on_log([](const dpp::log_t& event) {
        switch (event.severity) {
            case dpp::ll_critical:
                std::cout << "\033[1;31mCRITICAL: ";  // Bright Red
                break;
            case dpp::ll_error:
                std::cout << "\033[31mERROR: ";      // Red
                break;
            case dpp::ll_warning:
                std::cout << "\033[33mWARNING: ";    // Yellow
                break;
            case dpp::ll_info:
                std::cout << "\033[32mINFO: ";       // Green
                break;
            case dpp::ll_debug:
                std::cout << "\033[36mDEBUG: ";      // Cyan
                break;
        }
        std::cout << event.message << "\033[0m" << std::endl;  // Reset color at end
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

    event.reply("Joining your voice channel!");
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