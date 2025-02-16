#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include "config.hpp"
#include "inference.hpp"
#include "conversation.hpp"
#include "system_control.hpp"
#include "discord_bot.hpp"

// Function to handle console interaction
void runConsoleMode() {
    std::cout << "Digi-Ellie started. Type '/exit' to quit.\n";
    std::cout << "Please enter your name: ";
    std::string userName;
    std::getline(std::cin, userName);
    if (userName.empty()) userName = "User";

    std::string bootPrompt = buildPrompt("<Bootup>", "System");
    std::string initialResponse = runInference(bootPrompt);
    std::cout << "Ellie: " << initialResponse << std::endl;
    std::cout << "Ellie: Hello " << userName << "!\n";

    while (true) {
        std::cout << userName << ": ";
        std::string userInput;
        if (!std::getline(std::cin, userInput)) {
            break;
        }

        if (userInput == "/exit") {
            std::string exitPrompt = buildPrompt("<Shutdown>", "System");
            std::string ellieResponse = runInference(exitPrompt);
            std::cout << "Ellie: " << ellieResponse << std::endl;
            break;
        }

        if (userInput == "/clear") {
            clearHistory();
            std::cout << "Conversation history cleared.\n";
            continue;
        }

        std::string prompt = buildPrompt(userInput, userName);
        std::string ellieResponse = runInference(prompt);

        if (isSystemCommand(ellieResponse)) {
            handleSystemCommand(ellieResponse);
        } else {
            std::cout << "Ellie: " << ellieResponse << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (!initializeModel()) {
        std::cerr << "Error: Failed to load the model.\n";
        return 1;
    }

    // Check if Discord token is provided and select mode
    std::string discordToken = config::DISCORD_TOKEN;
    if (!discordToken.empty()) {
        std::cout << "Starting in Discord mode...\n";
        
        try {
            DiscordBot bot(discordToken);
            bot.start();
                 
            std::cout << "Bot has stopped, shutting down...\n";
            shutdownModel();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Discord bot error: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cout << "Discord token not provided, starting in console mode...\n";
        runConsoleMode();
    }

    shutdownModel();
    return 0;
}
