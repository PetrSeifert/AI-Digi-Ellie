#pragma once

#include <string>

void initializeConversation();
std::string buildPrompt(const std::string& userInput, const std::string& userName);
void addEllieResponse(const std::string& response);
void clearHistory();
