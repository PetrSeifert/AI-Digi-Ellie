#include "discord_bot/voice.hpp"
#include "logging.hpp"
#include <thread>
#include <chrono>

namespace discord {

    VoiceModule::VoiceModule(std::shared_ptr<CoreBot> core, std::shared_ptr<CommandsModule> commands) 
        : core(core), commands(commands), voice_connected(false), is_recording(false) {
        
        // Set up voice receive handler
        core->getBot()->on_voice_receive([this](const dpp::voice_receive_t& event) {
            if (is_recording) {
                auto& buffer = recording_buffers[event.user_id];
                size_t current_size = buffer.size();
                buffer.resize(current_size + event.audio_size);
                std::memcpy(buffer.data() + current_size, event.audio, event.audio_size);
                
                LOG_DEBUG("Recorded {} bytes of audio from user {} (total: {} bytes)", 
                        event.audio_size, event.user_id, buffer.size());
            }
        });

        registerCommands();
    }

    void VoiceModule::registerCommands() {
        auto bot = core->getBot();

        // Create the join voice command
        dpp::slashcommand joinVoiceCommand("joinvoice", "Join your current voice channel", bot->me.id);
        joinVoiceCommand.default_member_permissions = 0;
        commands->addCommand(std::move(joinVoiceCommand));
        commands->addCommandHandler("joinvoice", [this](const auto& event) { handleJoinVoiceCommand(event); });

        // Create the record command
        dpp::slashcommand recordCommand("record", "Start recording a user's voice", bot->me.id);
        recordCommand.default_member_permissions = 0;
        commands->addCommand(std::move(recordCommand));
        commands->addCommandHandler("record", [this](const auto& event) { handleRecordCommand(event); });

        // Create the stop recording command
        dpp::slashcommand stopRecordCommand("stoprecord", "Stop recording voice", bot->me.id);
        stopRecordCommand.default_member_permissions = 0;
        commands->addCommand(std::move(stopRecordCommand));
        commands->addCommandHandler("stoprecord", [this](const auto& event) { handleStopRecordCommand(event); });
    }

    void VoiceModule::handleJoinVoiceCommand(const dpp::slashcommand_t& event) {
        auto guild_id = event.command.guild_id;
        
        dpp::guild* g = dpp::find_guild(guild_id);
        if (!g) {
            event.reply("Failed to find the guild!");
            return;
        }

        if (!g->connect_member_voice(event.command.get_issuing_user().id)) {
            event.reply("You need to be in a voice channel first!");
            return;
        }

        voice_connected = true;
        event.reply("Joined your voice channel!");
    }

    void VoiceModule::handleRecordCommand(const dpp::slashcommand_t& event) {
        if (is_recording) {
            event.reply("Already recording! Stop the current recording first.");
            return;
        }

        dpp::guild* g = dpp::find_guild(event.command.guild_id);
        if (!g) {
            event.reply("Failed to find the guild!");
            return;
        }

        if (!voice_connected) {
            event.reply("Bot needs to be in a voice channel first! Use /joinvoice");
            return;
        }

        recording_buffers.clear();
        
        std::vector<dpp::snowflake> users_in_channel;
        for (const auto& [user_id, state] : g->voice_members) {
            recording_buffers[user_id].reserve(1024 * 1024); // Reserve 1MB initially
            users_in_channel.push_back(user_id);
        }

        if (users_in_channel.empty()) {
            event.reply("No users found in the voice channel!");
            return;
        }

        is_recording = true;
        
        std::string user_list;
        for (size_t i = 0; i < users_in_channel.size(); ++i) {
            if (i > 0) user_list += ", ";
            user_list += "<@" + std::to_string(users_in_channel[i]) + ">";
        }

        event.reply("Started recording users: " + user_list);
    }

    void VoiceModule::handleStopRecordCommand(const dpp::slashcommand_t& event) {
        if (!is_recording) {
            event.reply("Not currently recording!");
            return;
        }

        is_recording = false;
        
        std::stringstream ss;
        ss << "Stopped recording. Results:\n";
        event.reply(ss.str() + "...");
        
        for (const auto& [user_id, buffer] : recording_buffers) {
            size_t recorded_bytes = buffer.size();
            float recorded_seconds = static_cast<float>(recorded_bytes) / (48000.0f * 2.0f * 2.0f);
            int recorded_milliseconds = static_cast<int>(recorded_seconds * 1000.0f);
            
            ss << "<@" << user_id << ">: " << recorded_seconds << " seconds (" << recorded_bytes << " bytes)\n";
            
            if (auto vconn = core->getBot()->get_shard(0)->get_voice(event.command.guild_id)) {
                uint16_t* audio_data = reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(buffer.data()));
                size_t sample_count = buffer.size() / sizeof(uint16_t);
                
                if (sample_count > 0) {
                    LOG_INFO("Playing back {} samples of audio from user {}", sample_count, user_id);
                    vconn->voiceclient->send_audio_raw(audio_data, buffer.size());
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(recorded_milliseconds + 2000));
            }
        }
        
        event.edit_response(ss.str());
        recording_buffers.clear();
    }

    std::vector<uint16_t> VoiceModule::convertTTSAudioFormat(const std::vector<uint8_t>& mono_24khz) {
        std::vector<uint16_t> stereo_data;
        const int16_t* mono_samples = reinterpret_cast<const int16_t*>(mono_24khz.data());
        size_t num_samples = mono_24khz.size() / sizeof(int16_t);
        
        stereo_data.reserve(num_samples * 4);
        
        for (size_t i = 0; i < num_samples - 1; i++) {
            int16_t current = mono_samples[i];
            int16_t next = mono_samples[i + 1];
            
            stereo_data.push_back(static_cast<uint16_t>(current));
            stereo_data.push_back(static_cast<uint16_t>(current));
            
            int16_t interpolated = (current + next) / 2;
            stereo_data.push_back(static_cast<uint16_t>(interpolated));
            stereo_data.push_back(static_cast<uint16_t>(interpolated));
        }
        
        stereo_data.push_back(static_cast<uint16_t>(mono_samples[num_samples - 1]));
        stereo_data.push_back(static_cast<uint16_t>(mono_samples[num_samples - 1]));
        
        LOG_DEBUG("Converted {} mono samples to stereo", num_samples);
        
        return stereo_data;
    }

} // namespace discord 