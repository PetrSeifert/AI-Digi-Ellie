#include "inference.hpp"
#include "conversation.hpp"
#include "httplib.h"
#include "logging.hpp"

#include <string>
#include <map>
#include <random>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

static bool g_initialized = false;
static std::unique_ptr<httplib::Client> g_client;
static const int OLLAMA_PORT = 11434;

bool initializeModel() {
    try {
        g_client = std::make_unique<httplib::Client>(config::OLLAMA_HOST, OLLAMA_PORT);
        
        // Test connection and check if model exists
        auto response = g_client->Get("/api/tags");
        if (!response || response->status != 200) {
            LOG_CRITICAL("Failed to connect to Ollama server at {} : {}", config::OLLAMA_HOST, OLLAMA_PORT);
            return false;
        }

        g_initialized = true;
        LOG_INFO("Connected to Ollama server successfully!");
        
        // Initialize conversation with system prompt
        initializeConversation();
        return true;
    } catch (const std::exception& e) {
        LOG_CRITICAL("Error initializing Ollama client: {}", e.what());
        return false;
    }
}

std::string runInference(const std::string& conversationJson) {
    if (!g_initialized) {
        return "[Error: Ollama client not initialized]";
    }

    try {
        // Parse the conversation history from JSON
        json messages = json::parse(conversationJson);

        // Prepare the request body
        json request_body = {
            {"model", config::MODEL_NAME},
            {"messages", messages},
            {"stream", false}
        };

        // Make the request to Ollama
        auto response = g_client->Post("/api/chat", 
                                     request_body.dump(),
                                     "application/json");

        if (!response || response->status != 200) {
            LOG_ERROR("Error response: {}", (response ? response->body : "No response"));
            return "[Error: Failed to get response from Ollama]";
        }

        // Parse the response
        json response_json = json::parse(response->body);
        
        std::string ellieResponse;
        if (response_json.contains("message")) {
            ellieResponse = response_json["message"]["content"].get<std::string>();
            // Add the assistant's response to the conversation history
            addEllieResponse(ellieResponse);
            return ellieResponse;
        } else {
            return "[Error: Unexpected response format]";
        }

    } catch (const std::exception& e) {
        return std::string("[Error: ") + e.what() + "]";
    }
}

void shutdownModel() {
    g_client.reset();
    g_initialized = false;
    clearHistory();
}