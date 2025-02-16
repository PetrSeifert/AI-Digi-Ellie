#pragma once

#include "core.hpp"
#include <dpp/dpp.h>
#include <memory>
#include <vector>
#include <functional>

namespace discord {

    class CommandsModule {
    public:
        explicit CommandsModule(std::shared_ptr<CoreBot> core);
        
        void handleSlashCommand(const dpp::slashcommand_t& event);
        void clearCommand(const dpp::slashcommand_t& event);
        void shutdownCommand(const dpp::slashcommand_t& event);
        
        void registerCommands();
        void addCommand(dpp::slashcommand&& command);
        void addCommandHandler(const std::string& command_name, std::function<void(const dpp::slashcommand_t&)> handler);

    private:
        std::shared_ptr<CoreBot> core;
        std::vector<dpp::slashcommand> pending_commands;
        std::unordered_map<std::string, std::function<void(const dpp::slashcommand_t&)>> command_handlers;
    };

} // namespace discord 