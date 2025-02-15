#pragma once
#include <string>

bool isSystemCommand(const std::string& response);
void handleSystemCommand(const std::string& commandText);