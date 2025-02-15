#include "system_control.hpp"
#include <iostream>

bool isSystemCommand(const std::string& response) {
    return (response.find("[SYSTEM]:") != std::string::npos);
}

void handleSystemCommand(const std::string& commandText) {
    std::cout << "System command requested: " << commandText << std::endl;

    std::cout << "Allow? [y/N]: ";
    std::string decision;
    std::getline(std::cin, decision);

    if (decision == "y" || decision == "Y") {
        // Execute the command in real code. e.g. on Windows:
        // system("start notepad.exe");
        // or do more advanced calls
    } else {
        std::cout << "Command denied.\n";
    }
}
