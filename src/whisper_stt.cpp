#include "whisper_stt.hpp"
#include "logging.hpp"
#include <stdexcept>

WhisperSTT::WhisperSTT(const std::string& model_path) {
    // Initialize whisper context with default parameters
    struct whisper_context_params params = whisper_context_default_params();
    params.use_gpu = true;  // Enable GPU acceleration if available
    
    ctx = whisper_init_from_file_with_params(model_path.c_str(), params);
    if (!ctx) {
        throw std::runtime_error("Failed to initialize Whisper context from model: " + model_path);
    }
    
    // Initialize state
    state = whisper_init_state(ctx);
    if (!state) {
        whisper_free(ctx);
        throw std::runtime_error("Failed to initialize Whisper state");
    }
    
    LOG_INFO("Initialized Whisper STT with model: {}", model_path);
}

WhisperSTT::~WhisperSTT() {
    if (state) {
        whisper_free_state(state);
    }
    if (ctx) {
        whisper_free(ctx);
    }
}

std::vector<float> WhisperSTT::convertPCMToFloat(const std::vector<uint8_t>& audio_data) {
    // Convert 16-bit PCM to float
    const int16_t* pcm = reinterpret_cast<const int16_t*>(audio_data.data());
    size_t n_samples = audio_data.size() / sizeof(int16_t);
    
    std::vector<float> samples(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        // Convert to float and normalize to [-1, 1]
        samples[i] = static_cast<float>(pcm[i]) / 32768.0f;
    }
    
    return samples;
}

std::string WhisperSTT::audioToText(const std::vector<uint8_t>& audio_data) {
    if (audio_data.empty()) {
        LOG_WARN("Empty audio data provided to Whisper STT");
        return "";
    }
    
    // Convert PCM to float samples
    std::vector<float> samples = convertPCMToFloat(audio_data);
    
    // Set up parameters for full processing
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.translate = false;
    params.language = "en";  // Force English
    params.n_threads = 4;    // Use 4 threads for processing
    
    // Process the audio
    LOG_INFO("Processing {} samples with Whisper", samples.size());
    if (whisper_full_with_state(ctx, state, params, samples.data(), samples.size()) != 0) {
        LOG_ERROR("Failed to process audio with Whisper");
        return "";
    }
    
    // Get the number of segments
    const int n_segments = whisper_full_n_segments_from_state(state);
    if (n_segments <= 0) {
        LOG_WARN("No text segments found in audio");
        return "";
    }
    
    // Combine all segments into one string
    std::string result;
    for (int i = 0; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text_from_state(state, i);
        if (text) {
            if (!result.empty()) {
                result += " ";
            }
            result += text;
        }
    }
    
    LOG_INFO("Whisper transcription complete: {} segments, {} characters", n_segments, result.length());
    return result;
} 