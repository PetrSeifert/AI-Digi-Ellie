#pragma once

#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace discord {

    class CoreBot {
    public:
        CoreBot(const std::string& token);
        ~CoreBot();

        void start();
        void stop();
        bool isRunning() const { return is_running; }
        
        std::shared_ptr<dpp::cluster> getBot() const { return bot; }
        
        using ReadyCallback = std::function<void()>;
        void onReady(ReadyCallback callback);

    protected:
        void setupEvents();

        std::shared_ptr<dpp::cluster> bot;
        bool is_running;
        std::vector<ReadyCallback> ready_callbacks;
    };

} // namespace discord 