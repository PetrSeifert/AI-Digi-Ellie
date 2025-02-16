#pragma once

#include "core.hpp"
#include "azure_tts.hpp"
#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <vector>

namespace discord {

    class TTSModule {
    public:
        explicit TTSModule(std::shared_ptr<CoreBot> core, const std::string& azure_key, const std::string& azure_region);
        
        void speakText(const std::string& text, dpp::snowflake guild_id);
        bool isEnabled() const { return tts != nullptr; }

    private:
        std::shared_ptr<CoreBot> core;
        std::unique_ptr<AzureTTS> tts;
    };

} // namespace discord 