#include "discord_bot/voice.hpp"
#include "logging.hpp"
#include "config.hpp"
#include <thread>
#include <chrono>

namespace discord {

    VoiceModule::VoiceModule(std::shared_ptr<CoreBot> core, std::shared_ptr<CommandsModule> commands) 
        : core(core), commands(commands), voice_connected(false), is_recording(false) {
        
        // Initialize Azure STT if key is provided
        if (!config::AZURE_SPEECH_KEY.empty()) {
            stt = std::make_unique<AzureSTT>(config::AZURE_SPEECH_KEY, config::AZURE_SPEECH_REGION);
            LOG_INFO("STT module initialized with Azure credentials");
        } else {
            LOG_WARN("STT module initialized without Azure credentials - STT functionality will be disabled");
        }
        
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

    std::vector<uint8_t> VoiceModule::convertToWav(const std::vector<uint8_t>& raw_audio) {
        // WAV header for 48kHz 16-bit stereo PCM
        const uint8_t wav_header[] = {
            'R', 'I', 'F', 'F',                 // ChunkID
            0, 0, 0, 0,                         // ChunkSize (to be filled)
            'W', 'A', 'V', 'E',                 // Format
            'f', 'm', 't', ' ',                 // Subchunk1ID
            16, 0, 0, 0,                        // Subchunk1Size (16 for PCM)
            1, 0,                               // AudioFormat (1 for PCM)
            2, 0,                               // NumChannels (2 for stereo)
            0x80, 0xBB, 0, 0,                   // SampleRate (48000)
            0x00, 0xEE, 0x02, 0,                // ByteRate (48000 * 2 * 2)
            4, 0,                               // BlockAlign (2 * 2)
            16, 0,                              // BitsPerSample (16)
            'd', 'a', 't', 'a',                 // Subchunk2ID
            0, 0, 0, 0                          // Subchunk2Size (to be filled)
        };

        std::vector<uint8_t> wav_data;
        wav_data.reserve(sizeof(wav_header) + raw_audio.size());
        
        // Copy header
        wav_data.insert(wav_data.end(), wav_header, wav_header + sizeof(wav_header));
        
        // Copy audio data
        wav_data.insert(wav_data.end(), raw_audio.begin(), raw_audio.end());
        
        // Fill in sizes
        uint32_t data_size = raw_audio.size();
        uint32_t chunk_size = data_size + 36; // 36 = size of header minus 8
        
        // Write ChunkSize
        wav_data[4] = chunk_size & 0xFF;
        wav_data[5] = (chunk_size >> 8) & 0xFF;
        wav_data[6] = (chunk_size >> 16) & 0xFF;
        wav_data[7] = (chunk_size >> 24) & 0xFF;
        
        // Write Subchunk2Size
        wav_data[40] = data_size & 0xFF;
        wav_data[41] = (data_size >> 8) & 0xFF;
        wav_data[42] = (data_size >> 16) & 0xFF;
        wav_data[43] = (data_size >> 24) & 0xFF;
        
        return wav_data;
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
            
            ss << "<@" << user_id << ">: " << recorded_seconds << " seconds (" << recorded_bytes << " bytes)\n";
            
            if (recorded_bytes > 0) {
                try {
                    if (stt) {
                        // Convert raw PCM to WAV format
                        std::vector<uint8_t> wav_data = convertToWav(buffer);
                        
                        // Send to Azure STT
                        std::string transcribed_text = stt->audioToText(wav_data);
                        ss << "Transcription: " << transcribed_text << "\n\n";
                    } else {
                        ss << "STT not available - Azure credentials not configured\n\n";
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to transcribe audio: {}", e.what());
                    ss << "Failed to transcribe audio: " << e.what() << "\n\n";
                }
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