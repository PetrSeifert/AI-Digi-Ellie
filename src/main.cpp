#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include "config.hpp"
#include "inference.hpp"
#include "conversation.hpp"
#include "discord_bot.hpp"
#include "logging.hpp"

// Function to handle console interaction
void runConsoleMode() {
    LOG_INFO("Starting console mode");
    LOG_INFO("Digi-Ellie started. Type '/exit' to quit.");
    LOG_INFO("Please enter your name: ");
    std::string userName;
    std::getline(std::cin, userName);
    if (userName.empty()) userName = "User";
    LOG_INFO("User '{}' joined the session", userName);

    std::string bootPrompt = buildPrompt("<Bootup>", "System");
    std::string initialResponse = runInference(bootPrompt);
    LOG_DEBUG("Boot prompt response: {}", initialResponse);
    LOG_INFO("Ellie: {}", initialResponse);
    LOG_INFO("Ellie: Hello {}!", userName);

    while (true) {
        LOG_INFO("{}: ", userName);
        std::string userInput;
        if (!std::getline(std::cin, userInput)) {
            LOG_ERROR("Failed to read user input");
            break;
        }

        if (userInput == "/exit") {
            LOG_INFO("User requested exit");
            std::string exitPrompt = buildPrompt("<Shutdown>", "System");
            std::string ellieResponse = runInference(exitPrompt);
            LOG_INFO("Ellie: {}", ellieResponse);
            break;
        }

        if (userInput == "/clear") {
            LOG_INFO("Clearing conversation history");
            clearHistory();
            LOG_INFO("Conversation history cleared.");
            continue;
        }

        LOG_DEBUG("Processing user input: {}", userInput);
        std::string prompt = buildPrompt(userInput, userName);
        std::string ellieResponse = runInference(prompt);
        
        LOG_DEBUG("Ellie's response: {}", ellieResponse);
        LOG_INFO("Ellie: {}", ellieResponse);
    }
}

int main(int argc, char* argv[]) {
    // Initialize logging
    logging::init();
    LOG_INFO("Digi-Ellie starting up");

    if (!initializeModel()) {
        LOG_CRITICAL("Failed to load the model");
        return 1;
    }
    LOG_INFO("Model initialized successfully");

    // Check if Discord token is provided and select mode
    std::string discordToken = config::DISCORD_TOKEN;
    if (!discordToken.empty()) {
        LOG_INFO("Discord token found, starting in Discord mode");
        LOG_INFO("Starting in Discord mode...");
        
        try {
            DiscordBot bot(discordToken);
            bot.start();
            
            LOG_INFO("Bot has stopped, shutting down...");
            shutdownModel();
            return 0;
        } catch (const std::exception& e) {
            LOG_CRITICAL("Discord bot error: {}", e.what());
            return 1;
        }
    } else {
        LOG_WARN("Discord token not provided, starting in console mode...");
        runConsoleMode();
    }

    LOG_INFO("Shutting down model");
    shutdownModel();
    LOG_INFO("Digi-Ellie shutdown complete");
    return 0;
}
