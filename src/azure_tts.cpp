#include "azure_tts.hpp"
#include "logging.hpp"
#include <sstream>

using namespace Microsoft::CognitiveServices::Speech;

AzureTTS::AzureTTS(const std::string& subscription_key, const std::string& region) {
    try {
        speech_config = SpeechConfig::FromSubscription(subscription_key, region);
        speech_config->SetSpeechSynthesisOutputFormat(SpeechSynthesisOutputFormat::Raw24Khz16BitMonoPcm);
        synthesizer = SpeechSynthesizer::FromConfig(speech_config);
        LOG_INFO("Azure TTS initialized successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize Azure TTS: {}", e.what());
        throw;
    }
}

AzureTTS::~AzureTTS() {}

std::vector<uint8_t> AzureTTS::textToSpeech(const std::string& text, const std::string& voice) {
    try {
        // Set the voice
        speech_config->SetSpeechSynthesisVoiceName(voice);

        // Prepare SSML
        std::stringstream ssml;
        ssml << "<speak version='1.0' xml:lang='en-US'>"
             << "<voice name='" << voice << "'>"
             << text
             << "</voice></speak>";

        LOG_DEBUG("SSML Request:\n{}", ssml.str());
        
        // Synthesize speech
        auto result = synthesizer->SpeakSsmlAsync(ssml.str()).get();

        // Check result
        if (result->Reason == ResultReason::SynthesizingAudioCompleted) {
            // Get audio data
            auto audio_data = result->GetAudioData();
            LOG_INFO("Successfully synthesized speech: {} bytes", audio_data->size());
            return *audio_data;
        }
        else if (result->Reason == ResultReason::Canceled) {
            auto cancellation_details = SpeechSynthesisCancellationDetails::FromResult(result);
            LOG_ERROR("Speech synthesis canceled: {}", cancellation_details->ErrorDetails);
            throw std::runtime_error("Speech synthesis canceled: " + cancellation_details->ErrorDetails);
        }
        else {
            LOG_ERROR("Speech synthesis failed with reason: {}", static_cast<int>(result->Reason));
            throw std::runtime_error("Speech synthesis failed");
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error in textToSpeech: {}", e.what());
        throw;
    }
} 