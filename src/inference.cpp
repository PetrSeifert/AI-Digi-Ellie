#include "inference.hpp"
#include "conversation.hpp"
#include "httplib.h"
#include "logging.hpp"

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static bool g_initialized = false;
static std::unique_ptr<httplib::Client> g_client;

static const int LLM_PORT = 8000;

bool initializeModel() {
    try {
        LOG_INFO("Initializing model: {}", config::MODEL_NAME);
        g_client = std::make_unique<httplib::Client>(config::LLM_HOST, LLM_PORT);
        g_client->set_connection_timeout(5, 0);   // 5s
        g_client->set_read_timeout(120, 0);       // long enough for cold start

        // Probe the server with a tiny chat request (works on any OpenAI-compatible server)
        json probe = {
            {"model",    config::MODEL_NAME},
            {"messages", json::array({ {{"role","user"},{"content","ping"}} })},
            {"max_tokens", 1},
            {"temperature", 0.0}
        };

        auto r = g_client->Post("/v1/chat/completions", probe.dump(), "application/json");
        if (!r || r->status != 200) {
            LOG_CRITICAL("Failed to reach llama.cpp server at {}:{}, status={}",
                         config::LLM_HOST, LLM_PORT, (r ? r->status : -1));
            return false;
        }

        g_initialized = true;
        LOG_INFO("Connected to llama.cpp server successfully!");

        // Your existing function that seeds the conversation with a system prompt
        initializeConversation();
        return true;

    } catch (const std::exception& e) {
        LOG_CRITICAL("Error initializing llama.cpp client: {}", e.what());
        return false;
    }
}

// --- INFERENCE --------------------------------------------------------

std::string runInference(const std::string& conversationJson) {
    if (!g_initialized) {
        return "[Error: LLM client not initialized]";
    }

    try {
        // conversationJson must be an array of {role, content} messages
        json messages = json::parse(conversationJson);

        // Build OpenAI-style Chat Completions payload
        json request_body = {
            {"model",       config::MODEL_NAME},
            {"messages",    messages},
            {"max_tokens",  16384},       // tune for latency
            {"temperature", 0.2},
            {"stream",      false}
            // If you ever need tools:
            // {"tools", tools_json}, {"tool_choice","auto"}
        };

        auto response = g_client->Post("/v1/chat/completions",
                                       request_body.dump(),
                                       "application/json");

        if (!response || response->status != 200) {
            LOG_ERROR("LLM error: {}", (response ? response->body : "No response"));
            return "[Error: Failed to get response from LLM]";
        }

        json resp = json::parse(response->body);

        // Parse OpenAI-style response
        std::string ellieResponse;
        if (resp.contains("choices") && !resp["choices"].empty()) {
            const auto& choice = resp["choices"][0];

            // Primary: chat format
            if (choice.contains("message") && choice["message"].contains("content")) {
                ellieResponse = choice["message"]["content"].get<std::string>();
            }
            // Fallback: some servers expose "text"
            else if (choice.contains("text")) {
                ellieResponse = choice["text"].get<std::string>();
            }
            // Tool-call only (no immediate text)
            else if (choice.contains("message") && choice["message"].contains("tool_calls")) {
                ellieResponse = ""; // your tool pipeline will handle this turn
            }
            else {
                LOG_ERROR("Unexpected choices[0] shape: {}", choice.dump());
                return "[Error: Unexpected response format]";
            }
        } else {
            LOG_ERROR("No choices in response: {}", resp.dump());
            return "[Error: Unexpected response format]";
        }

        // Append assistant turn to your conversation history
        addEllieResponse(ellieResponse);
        return ellieResponse.empty() ? "[OK]" : ellieResponse;

    } catch (const std::exception& e) {
        return std::string("[Error: ") + e.what() + "]";
    }
}

// --- SHUTDOWN ---------------------------------------------------------

void shutdownModel() {
    g_client.reset();
    g_initialized = false;
    clearHistory();
}
