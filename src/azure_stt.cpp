#include "azure_stt.hpp"
#include "logging.hpp"
#include <sstream>

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Audio;

AzureSTT::AzureSTT(const std::string& subscription_key, const std::string& region) {
    try {
        speech_config = SpeechConfig::FromSubscription(subscription_key, region);
        speech_config->SetSpeechRecognitionLanguage("en-US");
        recognizer = SpeechRecognizer::FromConfig(speech_config);
        LOG_INFO("Azure STT initialized successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize Azure STT: {}", e.what());
        throw;
    }
}

AzureSTT::~AzureSTT() {}

std::string AzureSTT::audioToText(const std::vector<uint8_t>& audio_data) {
    try {
        // Create memory stream from audio data
        auto audio_stream = AudioInputStream::CreatePushStream(
            AudioStreamFormat::GetWaveFormatPCM(48000, 16, 1)
        );

        // Push audio data to the stream
        std::vector<uint8_t> mutable_data(audio_data);
        audio_stream->Write(mutable_data.data(), mutable_data.size());
        audio_stream->Close();

        // Create audio config from the stream
        auto audio_config = AudioConfig::FromStreamInput(audio_stream);
        
        // Create a new recognizer with the audio config
        auto stream_recognizer = SpeechRecognizer::FromConfig(speech_config, audio_config);

        LOG_INFO("Starting speech recognition...");
        
        // Start recognition
        auto result = stream_recognizer->RecognizeOnceAsync().get();

        // Check result
        if (result->Reason == ResultReason::RecognizedSpeech) {
            LOG_INFO("Successfully transcribed audio to text: {}", result->Text);
            return result->Text;
        }
        else if (result->Reason == ResultReason::NoMatch) {
            auto no_match_details = NoMatchDetails::FromResult(result);
            LOG_ERROR("Speech recognition failed: {}", static_cast<int>(no_match_details->Reason));
            throw std::runtime_error("No speech could be recognized");
        }
        else if (result->Reason == ResultReason::Canceled) {
            auto cancellation_details = CancellationDetails::FromResult(result);
            LOG_ERROR("Speech recognition canceled: {}", cancellation_details->ErrorDetails);
            throw std::runtime_error("Speech recognition canceled: " + cancellation_details->ErrorDetails);
        }
        else {
            LOG_ERROR("Speech recognition failed with reason: {}", static_cast<int>(result->Reason));
            throw std::runtime_error("Speech recognition failed");
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error in audioToText: {}", e.what());
        throw;
    }
} 