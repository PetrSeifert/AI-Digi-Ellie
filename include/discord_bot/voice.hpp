#pragma once

#include "core.hpp"
#include "commands.hpp"
#include "whisper_client.hpp"
#include "azure_tts.hpp"
#include <dpp/dpp.h>
#include <vector>
#include <map>
#include <cstdint>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace discord {

	struct UserAudioState {
		std::vector<uint8_t> current_buffer;
		std::chrono::steady_clock::time_point last_audio_time;
		bool is_speaking;
		dpp::snowflake guild_id;
	};

	class VoiceModule {
	public:
		explicit VoiceModule(std::shared_ptr<CoreBot> core, std::shared_ptr<CommandsModule> commands);
		~VoiceModule();
		
		void handleJoinVoiceCommand(const dpp::slashcommand_t& event);
		
		bool isVoiceConnected() const { return voice_connected; }
		bool isRecording() const { return is_recording; }
		
		static std::vector<uint16_t> convertTTSAudioFormat(const std::vector<uint8_t>& mono_24khz);
		void sendVoiceMessage(dpp::snowflake user_id, const std::string& text, dpp::snowflake guild_id);

	private:
		void registerCommands();
		std::vector<uint8_t> convertToWav(const std::vector<uint8_t>& raw_audio);
		void checkForSilenceAndTranscribe(dpp::snowflake user_id);
		void processAudioChunk(dpp::snowflake user_id, const uint8_t* audio, size_t audio_size);
		void speakText(const std::string& text, dpp::snowflake guild_id);
		void startSilenceDetectionTimer();
		void stopSilenceDetectionTimer();
		void silenceDetectionLoop();

		struct StreamSessionState {
			std::vector<uint8_t> accumulator;
			std::string committed_text;
			std::string pending_fragment;
			std::string last_partial;
		};

		void mergePartialTranscript(StreamSessionState& state, const std::string& partial_text);

		// Streaming state per user
		std::unordered_map<dpp::snowflake, std::string> user_stream_session_ids;
		std::unordered_map<dpp::snowflake, StreamSessionState> stream_states;

		std::shared_ptr<CoreBot> core;
		std::shared_ptr<CommandsModule> commands;
		std::unique_ptr<WhisperClient> stt;
		std::unique_ptr<AzureTTS> tts;
		bool voice_connected;
		bool is_recording;
		std::map<dpp::snowflake, UserAudioState> user_audio_states;
		
		std::thread silence_detection_thread;
		std::atomic<bool> should_stop_silence_detection;
		std::mutex audio_states_mutex;
		
		static constexpr std::chrono::milliseconds SILENCE_THRESHOLD{500};
		static constexpr std::chrono::milliseconds SILENCE_CHECK_INTERVAL{100};
		static constexpr size_t MIN_AUDIO_SIZE{20000}; // Minimum audio size to process (about 0.1s at 48kHz stereo)

		// Streaming helpers (1s min chunk for robust partials at 48k stereo 16-bit)
		static constexpr size_t MIN_STREAM_SEND_BYTES{194000};
		static constexpr size_t OVERLAP_CHARS{16};
	};

} // namespace discord 