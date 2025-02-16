#include "discord_bot/message.hpp"
#include "logging.hpp"
#include "conversation.hpp"
#include "inference.hpp"
#include <iomanip>

namespace discord {

    MessageModule::MessageModule(std::shared_ptr<CoreBot> core)
        : core(core) {
        
        // Set up message handler
        core->getBot()->on_message_create([this](const dpp::message_create_t& event) {
            handleMessage(event);
        });
    }

    void MessageModule::handleMessage(const dpp::message_create_t& event) {
        // Ignore messages from bots (including ourselves)
        if (event.msg.author.is_bot()) {
            return;
        }

        // Get the user's name and message
        std::string userName = event.msg.author.username;
        std::string userMessage = event.msg.content;

        // Debug output
        LOG_INFO("=== Incoming Discord Message ===");
        LOG_INFO("From: {} (ID: {})", userName, event.msg.author.id);
        LOG_INFO("Channel: {}", event.msg.channel_id);
        LOG_INFO("Content: {}", userMessage);
        LOG_INFO("Message Length: {}", userMessage.length());
        LOG_DEBUG("Raw Content Bytes: {}", [&userMessage]() {
            std::stringstream ss;
            for (unsigned char c : userMessage) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
            }
            return ss.str();
        }());
        LOG_INFO("==============================");

        // Build the prompt from the user's message
        std::string prompt = buildPrompt(userMessage, userName);
        
        // Debug output for prompt
        LOG_DEBUG("=== Generated Prompt ===");
        LOG_DEBUG("{}", prompt);
        LOG_DEBUG("=====================");

        // Get Ellie's response
        std::string ellieResponse = runInference(prompt);

        // Debug output for response
        LOG_INFO("=== Ellie's Response ===");
        LOG_INFO("{}", ellieResponse);
        LOG_INFO("==================");

        // Check if the response is not empty
        if (!ellieResponse.empty()) {
            // Notify all response callbacks
            for (const auto& callback : response_callbacks) {
                callback(ellieResponse, event.msg.guild_id);
            }

            // Send the response back to Discord
            sendMessage(event.msg.channel_id, ellieResponse);
        } else {
            LOG_ERROR("Ellie did not respond to the message.");
        }
    }

    void MessageModule::sendMessage(dpp::snowflake channel_id, const std::string& message) {
        if (message.length() > MAX_MESSAGE_LENGTH) {
            LOG_INFO("Response exceeds Discord limit, splitting into chunks...");
            splitAndSendLongMessage(channel_id, message);
        } else {
            core->getBot()->message_create(dpp::message(channel_id, message));
        }
    }

    void MessageModule::splitAndSendLongMessage(dpp::snowflake channel_id, const std::string& message) {
        for (size_t i = 0; i < message.length(); i += MAX_MESSAGE_LENGTH) {
            std::string chunk = message.substr(i, MAX_MESSAGE_LENGTH);
            core->getBot()->message_create(dpp::message(channel_id, chunk));
            LOG_DEBUG("Sent chunk of size: {}", chunk.length());
        }
    }

} // namespace discord 