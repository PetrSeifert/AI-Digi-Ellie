#include "discord_bot/core.hpp"
#include "logging.hpp"
#include "config.hpp"
#include "conversation.hpp"
#include "inference.hpp"

namespace discord {

    CoreBot::CoreBot(const std::string& token) : is_running(false) {
        LOG_INFO("Creating Discord bot core...");
        
        // Set up intents
        uint32_t intents = dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_messages | dpp::i_direct_messages;
        bot = std::make_shared<dpp::cluster>(token, intents);
        
        LOG_INFO("Bot instance created with intents: 0x{:x}", intents);
        LOG_INFO("Make sure Message Content Intent is enabled in Discord Developer Portal!");
        
        setupEvents();
    }

    CoreBot::~CoreBot() {
        stop();
    }

    void CoreBot::onReady(ReadyCallback callback) {
        ready_callbacks.push_back(std::move(callback));
    }

    void CoreBot::setupEvents() {
        if (!bot) {
            LOG_ERROR("Error: Bot instance is null during event setup!");
            return;
        }

        // Log when bot is ready
        bot->on_ready([this](const dpp::ready_t& event) {
            LOG_INFO("=== Bot Ready Event ===");
            LOG_INFO("Logged in as: {}", bot->me.username);
            LOG_INFO("Bot ID: {}", bot->me.id);
            LOG_INFO("=====================");

            // Send boot message to Ellie
            std::string bootPrompt = buildPrompt("<Bootup>", "System");
            std::string bootResponse = runInference(bootPrompt);
            bot->message_create(dpp::message(dpp::snowflake(config::DEFAULT_CHANNEL_ID), bootResponse));

            // Execute all ready callbacks
            for (const auto& callback : ready_callbacks) {
                callback();
            }
        });

        // Add log handler
        bot->on_log([](const dpp::log_t& event) {
            if (event.severity == dpp::ll_critical) {
                LOG_CRITICAL(event.message);
            } else if (event.severity == dpp::ll_error) {
                LOG_ERROR(event.message);
            } else if (event.severity == dpp::ll_warning) {
                LOG_WARN(event.message);
            } else if (event.severity == dpp::ll_debug) {
                LOG_DEBUG(event.message);
            } else {
                LOG_INFO(event.message);
            }
        });
    }

    void CoreBot::start() {
        if (!is_running) {
            LOG_INFO("Starting Discord bot core...");
            is_running = true;
            try {
                bot->start(dpp::st_wait);
            } catch (const std::exception& e) {
                LOG_ERROR("Error starting bot: {}", e.what());
                is_running = false;
            }
        }
    }

    void CoreBot::stop() {
        if (is_running) {
            LOG_INFO("Stopping Discord bot core...");
            is_running = false;
            bot->shutdown();
            LOG_INFO("Bot core stopped.");
        }
    }

} // namespace discord 