#pragma once

#include "core.hpp"
#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace discord {

    class MessageModule {
    public:
        explicit MessageModule(std::shared_ptr<CoreBot> core);
        
        void handleMessage(const dpp::message_create_t& event);
        void sendMessage(dpp::snowflake channel_id, const std::string& message);

        // Event signals that other modules can listen to
        using ResponseCallback = std::function<void(const std::string& response, dpp::snowflake guild_id)>;
        void onResponse(ResponseCallback callback) { response_callbacks.push_back(std::move(callback)); }

    private:
        std::shared_ptr<CoreBot> core;
        std::vector<ResponseCallback> response_callbacks;
        
        void splitAndSendLongMessage(dpp::snowflake channel_id, const std::string& message);
        static constexpr size_t MAX_MESSAGE_LENGTH = 2000;
    };

} // namespace discord 