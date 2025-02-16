#include "discord_bot/commands.hpp"
#include "logging.hpp"
#include "conversation.hpp"
#include "inference.hpp"

namespace discord {

    CommandsModule::CommandsModule(std::shared_ptr<CoreBot> core) : core(core) {
        auto bot = core->getBot();

        // Set up slash command handler
        bot->on_slashcommand([this](const dpp::slashcommand_t& event) {
            handleSlashCommand(event);
        });
        
        // Create the clear command
        dpp::slashcommand clearCommand("clear", "Clear the conversation history", bot->me.id);
        clearCommand.default_member_permissions = 0;
        addCommand(std::move(clearCommand));
        addCommandHandler("clear", [this](const auto& event) { this->clearCommand(event); });
        
        // Create the shutdown command
        dpp::slashcommand shutdownCommand("shutdown", "Shutdown the bot", bot->me.id);
        shutdownCommand.default_member_permissions = 0;
        addCommand(std::move(shutdownCommand));
        addCommandHandler("shutdown", [this](const auto& event) { this->shutdownCommand(event); });
    }

    void CommandsModule::addCommand(dpp::slashcommand&& command) {
        pending_commands.push_back(std::move(command));
    }

    void CommandsModule::addCommandHandler(const std::string& command_name, 
                                        std::function<void(const dpp::slashcommand_t&)> handler) {
        command_handlers[command_name] = std::move(handler);
    }

    void CommandsModule::registerCommands() {
        auto bot = core->getBot();
        if (!bot || pending_commands.empty()) return;

        LOG_INFO("Registering {} commands for application ID: {}", pending_commands.size(), bot->me.id);

        bot->global_bulk_command_create(pending_commands, [this](const dpp::confirmation_callback_t& callback) {
            if (callback.is_error()) {
                LOG_ERROR("Error registering commands: {}", callback.get_error().message);
            } else {
                LOG_INFO("Successfully registered {} slash commands.", pending_commands.size());
            }
        });
    }

    void CommandsModule::handleSlashCommand(const dpp::slashcommand_t& event) {
        const std::string& commandName = event.command.get_command_name();
        
        auto it = command_handlers.find(commandName);
        if (it != command_handlers.end()) {
            it->second(event);
        } else {
            LOG_WARN("Received unknown command: {}", commandName);
        }
    }

    void CommandsModule::clearCommand(const dpp::slashcommand_t& event) {
        clearHistory();
        event.reply("Conversation history cleared.");
    }

    void CommandsModule::shutdownCommand(const dpp::slashcommand_t& event) {
        event.reply("Shutting down...");
        // Send shutdown message to Ellie
        std::string shutdownPrompt = buildPrompt("<Shutdown>", "System");
        std::string shutdownResponse = runInference(shutdownPrompt);
        
        event.edit_response(shutdownResponse, [this](const dpp::confirmation_callback_t& callback) {
            core->stop();
        });
    }

} // namespace discord 