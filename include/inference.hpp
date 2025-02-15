#pragma once
#include <string>
#include "config.hpp"

bool initializeModel();
std::string runInference(const std::string& conversationJson);
void shutdownModel();