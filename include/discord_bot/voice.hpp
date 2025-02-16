#pragma once

#include "core.hpp"
#include "commands.hpp"
#include "azure_stt.hpp"
#include <dpp/dpp.h>
#include <vector>
#include <map>
#include <cstdint>

namespace discord {

    class VoiceModule {
    public:
        explicit VoiceModule(std::shared_ptr<CoreBot> core, std::shared_ptr<CommandsModule> commands);
        
        void handleJoinVoiceCommand(const dpp::slashcommand_t& event);
        void handleRecordCommand(const dpp::slashcommand_t& event);
        void handleStopRecordCommand(const dpp::slashcommand_t& event);
        
        bool isVoiceConnected() const { return voice_connected; }
        bool isRecording() const { return is_recording; }
        
        static std::vector<uint16_t> convertTTSAudioFormat(const std::vector<uint8_t>& mono_24khz);

    private:
        void registerCommands();
        std::vector<uint8_t> convertToWav(const std::vector<uint8_t>& raw_audio);

        std::shared_ptr<CoreBot> core;
        std::shared_ptr<CommandsModule> commands;
        std::unique_ptr<AzureSTT> stt;
        bool voice_connected;
        bool is_recording;
        std::map<dpp::snowflake, std::vector<uint8_t>> recording_buffers;
    };

} // namespace discord 