#include "system_control.hpp"
#include "logging.hpp"
#include <iostream>

bool isSystemCommand(const std::string& response) {
    return (response.find("[SYSTEM]:") != std::string::npos);
}

void handleSystemCommand(const std::string& commandText) {
    LOG_INFO("System command requested: {}", commandText);

    LOG_INFO("Allow? [y/N]: ");
    std::string decision;
    std::getline(std::cin, decision);

    if (decision == "y" || decision == "Y") {
        // Execute the command in real code. e.g. on Windows:
        // system("start notepad.exe");
        // or do more advanced calls
    } else {
        LOG_INFO("Command denied.");
    }
}
