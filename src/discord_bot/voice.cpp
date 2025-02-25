#include "discord_bot/voice.hpp"
#include "logging.hpp"
#include "config.hpp"
#include "inference.hpp"
#include "conversation.hpp"
#include <thread>
#include <chrono>

namespace discord {

    VoiceModule::VoiceModule(std::shared_ptr<CoreBot> core, std::shared_ptr<CommandsModule> commands) 
        : core(core), commands(commands), voice_connected(false), is_recording(false),
          should_stop_silence_detection(true) {
        
        // Initialize Whisper client
        try {
            std::string service_url = std::string("http://") + config::WHISPER_SERVICE_HOST + ":" + std::to_string(config::WHISPER_SERVICE_PORT);
            stt = std::make_unique<WhisperClient>(service_url, 5000);  // 5 second delay between reconnection attempts
            
            if (stt->isHealthy()) {
                LOG_INFO("STT module initialized and connected to Whisper service");
            } else {
                LOG_WARN("Whisper service is not healthy, background reconnection task started");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize Whisper client: {}", e.what());
            LOG_WARN("STT functionality will be disabled");
        }

        // Initialize Azure TTS if key is provided
        if (!config::AZURE_SPEECH_KEY.empty()) {
            tts = std::make_unique<AzureTTS>(config::AZURE_SPEECH_KEY, config::AZURE_SPEECH_REGION);
            LOG_INFO("TTS module initialized with Azure credentials");
        } else {
            LOG_WARN("TTS module initialized without Azure credentials - TTS functionality will be disabled");
        }
        
        // Set up voice receive handler
        core->getBot()->on_voice_receive([this](const dpp::voice_receive_t& event) {
            if (voice_connected) {
                std::lock_guard<std::mutex> lock(audio_states_mutex);
                processAudioChunk(event.user_id, event.audio, event.audio_size);
            }
        });

        registerCommands();
    }

    VoiceModule::~VoiceModule() {
        stopSilenceDetectionTimer();
    }

    void VoiceModule::startSilenceDetectionTimer() {
        should_stop_silence_detection = false;
        silence_detection_thread = std::thread(&VoiceModule::silenceDetectionLoop, this);
        LOG_INFO("Started silence detection timer");
    }

    void VoiceModule::stopSilenceDetectionTimer() {
        should_stop_silence_detection = true;
        if (silence_detection_thread.joinable()) {
            silence_detection_thread.join();
        }
        LOG_INFO("Stopped silence detection timer");
    }

    void VoiceModule::silenceDetectionLoop() {
        while (!should_stop_silence_detection) {
            {
                std::lock_guard<std::mutex> lock(audio_states_mutex);
                for (const auto& [user_id, _] : user_audio_states) {
                    checkForSilenceAndTranscribe(user_id);
                }
            }
            std::this_thread::sleep_for(SILENCE_CHECK_INTERVAL);
        }
    }

    void VoiceModule::processAudioChunk(dpp::snowflake user_id, const uint8_t* audio, size_t audio_size) {
        auto& state = user_audio_states[user_id];
        auto now = std::chrono::steady_clock::now();

        // If we haven't received audio for this user before, initialize their state
        if (state.current_buffer.empty()) {
            state.last_audio_time = now;
            state.is_speaking = false;
        }

        // Check if this is a new speech segment
        if (!state.is_speaking && audio_size > 0) {
            state.is_speaking = true;
            state.current_buffer.clear();
            LOG_DEBUG("User {} started speaking", user_id);
        }

        // Add audio to buffer if we're recording or the user is speaking
        if (is_recording || state.is_speaking) {
            size_t current_size = state.current_buffer.size();
            state.current_buffer.resize(current_size + audio_size);
            std::memcpy(state.current_buffer.data() + current_size, audio, audio_size);
            state.last_audio_time = now;
            
            LOG_DEBUG("Recorded {} bytes of audio from user {} (total: {} bytes)", 
                    audio_size, user_id, state.current_buffer.size());
        }
    }

    void VoiceModule::sendVoiceMessage(dpp::snowflake user_id, const std::string& message, dpp::snowflake guild_id) {
        dpp::guild* g = dpp::find_guild(guild_id);
        dpp::user* u = dpp::find_user(user_id);
        std::string username = u->username;

        std::string prompt = buildPrompt(message, username);
        
        // Debug output for prompt
        LOG_DEBUG("=== Generated Prompt ===");
        LOG_DEBUG("{}", prompt);
        LOG_DEBUG("=====================");

        // Get Ellie's response
        std::string ellieResponse = runInference(prompt);

        // Debug output for response
        LOG_INFO("=== Ellie's Response ===");
        LOG_INFO("{}", ellieResponse);
        LOG_INFO("==================");

        // Check if the response is not empty
        if (!ellieResponse.empty()) {
            speakText(ellieResponse, guild_id);
        } else {
            LOG_ERROR("Ellie did not respond to the message.");
        }
    }

    void VoiceModule::checkForSilenceAndTranscribe(dpp::snowflake user_id) {
        auto& state = user_audio_states[user_id];
        auto now = std::chrono::steady_clock::now();
        auto silence_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - state.last_audio_time
        );

        // If we've detected silence and have enough audio data
        if (state.is_speaking && 
            silence_duration > SILENCE_THRESHOLD && 
            state.current_buffer.size() >= MIN_AUDIO_SIZE) {
            
            state.is_speaking = false;
            LOG_INFO("Detected silence for user {}, transcribing {} bytes", 
                    user_id, state.current_buffer.size());

            try {
                if (stt) {
                    // Send to Whisper service
                    std::string transcribed_text;
                    try {
                        transcribed_text = stt->audioToText(state.current_buffer);
                    } catch (const std::exception& e) {
                        LOG_ERROR("Error during transcription: {}", e.what());
                        LOG_INFO("Whisper service unavailable, transcription will be retried when service is back online");
                        // No need to manually reconnect, the background task is handling it
                        throw;  // Re-throw to be caught by outer catch block
                    }

                    if (!transcribed_text.empty()) {
                        // Send the transcribed text to Ellie
                        sendVoiceMessage(user_id, transcribed_text, state.guild_id);
                    }
                    LOG_INFO("Transcription for user {}: {}", user_id, transcribed_text);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to transcribe audio for user {}: {}", user_id, e.what());
            }

            // Clear the buffer after transcription
            state.current_buffer.clear();
        }
    }
    
    void VoiceModule::speakText(const std::string& text, dpp::snowflake guild_id) {
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

    void VoiceModule::registerCommands() {
        auto bot = core->getBot();

        // Create the join voice command
        dpp::slashcommand joinVoiceCommand("joinvoice", "Join your current voice channel", bot->me.id);
        joinVoiceCommand.default_member_permissions = 0;
        commands->addCommand(std::move(joinVoiceCommand));
        commands->addCommandHandler("joinvoice", [this](const auto& event) { handleJoinVoiceCommand(event); });
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
        
        // Initialize states for all users in the voice channel
        {
            std::lock_guard<std::mutex> lock(audio_states_mutex);
            user_audio_states.clear();
            for (const auto& [user_id, state] : g->voice_members) {
                UserAudioState audio_state;
                audio_state.current_buffer.reserve(1024 * 1024); // Reserve 1MB initially
                audio_state.is_speaking = false;
                audio_state.last_audio_time = std::chrono::steady_clock::now();
                audio_state.guild_id = guild_id;
                user_audio_states[user_id] = std::move(audio_state);
            }
        }
        
        startSilenceDetectionTimer();
        event.reply("Joined your voice channel! I will now transcribe speech automatically when there are pauses in conversation.");
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