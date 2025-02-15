#include <iostream>
#include <string>
#include <map>
#include "inference.hpp"
#include "conversation.hpp"
#include "system_control.hpp"

int main() {
    if (!initializeModel()) {
        std::cerr << "Error: Failed to load the model.\n";
        return 1;
    }

    std::cout << "AI Assistant Ellie started. Type 'exit' to quit.\n";
    std::cout << "Please enter your name: ";
    std::string userName;
    std::getline(std::cin, userName);
    if (userName.empty()) userName = "User";

    std::string bootPrompt = buildPrompt("<Bootup>", "");
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
            std::string exitPrompt = buildPrompt("<Shutdown>", "");
            
            std::string aiResponse = runInference(exitPrompt);
            std::cout << "Ellie: " << aiResponse << std::endl;
            break;
        }

        if (userInput == "/clear") {
            clearHistory();
            std::cout << "Conversation history cleared.\n";
            continue;
        }

        std::string prompt = buildPrompt(userInput, userName);
        
        std::string aiResponse = runInference(prompt);

        if (isSystemCommand(aiResponse)) {
            handleSystemCommand(aiResponse);
        } else {
            std::cout << "Ellie: " << aiResponse << std::endl;
        }
    }

    shutdownModel();
    return 0;
}
