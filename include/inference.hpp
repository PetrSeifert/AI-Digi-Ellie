#pragma once
#include <string>

bool initializeModel();
std::string runInference(const std::string& conversationJson);
void shutdownModel();
