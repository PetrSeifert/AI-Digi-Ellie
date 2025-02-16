#include "discord_bot/tts.hpp"
#include "discord_bot/voice.hpp"
#include "logging.hpp"
#include "config.hpp"
#include "inference.hpp"

namespace discord {

    TTSModule::TTSModule(std::shared_ptr<CoreBot> core, const std::string& azure_key, const std::string& azure_region)
        : core(core) {
        if (!azure_key.empty()) {
            tts = std::make_unique<AzureTTS>(azure_key, azure_region);
            LOG_INFO("TTS module initialized with Azure credentials");
        } else {
            LOG_WARN("TTS module initialized without Azure credentials - TTS functionality will be disabled");
        }
    }

    void TTSModule::speakText(const std::string& text, dpp::snowflake guild_id) {
        if (!tts) {
            LOG_WARN("TTS not initialized - missing Azure key");
            return;
        }

        try {
            // Convert text to speech
            std::vector<uint8_t> audio_data = tts->textToSpeech(text, config::AZURE_SPEECH_VOICE);
            
            // Get voice connection for the guild
            if (auto vconn = core->getBot()->get_shard(0)->get_voice(guild_id)) {
                // Convert audio format and send
                std::vector<uint16_t> stereo_data = VoiceModule::convertTTSAudioFormat(audio_data);
                vconn->voiceclient->send_audio_raw(stereo_data.data(), stereo_data.size() * sizeof(uint16_t));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error in TTS: {}", e.what());
        }
    }

} // namespace discord 