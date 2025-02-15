#include "inference.hpp"
#include "conversation.hpp"
#include <string>
#include <map>
#include <random>
#include <algorithm>
#include <cctype>
#include <iostream>
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

static bool g_initialized = false;
static std::unique_ptr<httplib::Client> g_client;
static const char* OLLAMA_HOST = "localhost";
static const int OLLAMA_PORT = 11434;

bool initializeModel() {
    try {
        g_client = std::make_unique<httplib::Client>(OLLAMA_HOST, OLLAMA_PORT);
        
        // Test connection and check if model exists
        auto response = g_client->Get("/api/tags");
        if (!response || response->status != 200) {
            std::cerr << "Failed to connect to Ollama server at " << OLLAMA_HOST << ":" << OLLAMA_PORT << std::endl;
            return false;
        }

        g_initialized = true;
        std::cout << "Connected to Ollama server successfully!\n";
        
        // Initialize conversation with system prompt
        initializeConversation();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing Ollama client: " << e.what() << std::endl;
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
            {"model", "mistral"},
            {"messages", messages},
            {"stream", false}
        };

        // Make the request to Ollama
        auto response = g_client->Post("/api/chat", 
                                     request_body.dump(),
                                     "application/json");

        if (!response || response->status != 200) {
            std::cerr << "Error response: " << (response ? response->body : "No response") << std::endl;
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