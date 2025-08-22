#include "audio_utils.hpp"
#include "logging.hpp"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace audio_utils {

// WAV file header structure
struct WavHeader {
    // RIFF chunk
    char riff_id[4] = {'R', 'I', 'F', 'F'};
    uint32_t riff_size;
    char wave_id[4] = {'W', 'A', 'V', 'E'};
    
    // fmt sub-chunk
    char fmt_id[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // PCM = 1
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    
    // data sub-chunk
    char data_id[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;
};

std::vector<uint8_t> stereoToMono(const std::vector<uint8_t>& stereo_data) {
    if (stereo_data.empty() || stereo_data.size() % 4 != 0) {
        LOG_ERROR("Invalid stereo data for conversion to mono");
        return {};
    }
    
    // Each stereo sample is 4 bytes (2 bytes for left channel, 2 bytes for right channel)
    size_t stereo_samples = stereo_data.size() / 4;
    std::vector<uint8_t> mono_data(stereo_samples * 2); // Each mono sample is 2 bytes
    
    LOG_INFO("Converting stereo to mono: {} stereo samples -> {} mono samples", 
             stereo_samples, stereo_samples);
    
    // Process each stereo sample (left and right channels)
    for (size_t i = 0; i < stereo_samples; i++) {
        // Get left and right channel samples as int16_t
        const int16_t* stereo_samples_ptr = reinterpret_cast<const int16_t*>(stereo_data.data());
        int16_t left = stereo_samples_ptr[i * 2];     // Left channel
        int16_t right = stereo_samples_ptr[i * 2 + 1]; // Right channel
        
        // Average the left and right channels
        int32_t avg = (static_cast<int32_t>(left) + static_cast<int32_t>(right)) / 2;
        
        // Clamp to int16_t range
        if (avg > 32767) avg = 32767;
        if (avg < -32768) avg = -32768;
        
        // Store the mono sample
        int16_t mono_sample = static_cast<int16_t>(avg);
        int16_t* mono_samples_ptr = reinterpret_cast<int16_t*>(mono_data.data());
        mono_samples_ptr[i] = mono_sample;
    }
    
    LOG_INFO("Stereo to mono conversion complete: {} bytes -> {} bytes", 
             stereo_data.size(), mono_data.size());
    return mono_data;
}

std::vector<uint8_t> downsamplePCM(const std::vector<uint8_t>& input_data, int input_sample_rate, int output_sample_rate) {
    if (input_data.empty() || input_sample_rate <= 0 || output_sample_rate <= 0) {
        LOG_ERROR("Invalid input for downsampling");
        return {};
    }
    
    // If sample rates are the same, return a copy of the input
    if (input_sample_rate == output_sample_rate) {
        LOG_INFO("No downsampling needed, sample rates are the same");
        return input_data;
    }
    
    // Calculate the ratio between input and output sample rates
    double ratio = static_cast<double>(input_sample_rate) / output_sample_rate;
    
    // Convert input bytes to int16_t samples
    const int16_t* input_samples = reinterpret_cast<const int16_t*>(input_data.data());
    size_t input_sample_count = input_data.size() / sizeof(int16_t);
    
    // Calculate output sample count
    size_t output_sample_count = static_cast<size_t>(std::ceil(input_sample_count / ratio));
    
    // Create output buffer for int16_t samples
    std::vector<int16_t> output_samples(output_sample_count);
    
    LOG_INFO("Downsampling from {}Hz to {}Hz (ratio: {})", input_sample_rate, output_sample_rate, ratio);
    LOG_INFO("Input samples: {}, Output samples: {}", input_sample_count, output_sample_count);
    
    // Simple linear interpolation downsampling
    for (size_t i = 0; i < output_sample_count; i++) {
        // Calculate the corresponding position in the input
        double input_pos = i * ratio;
        size_t input_idx = static_cast<size_t>(input_pos);
        
        // Ensure we don't go out of bounds
        if (input_idx >= input_sample_count) {
            input_idx = input_sample_count - 1;
        }
        
        // Simple point sampling (take the nearest sample)
        output_samples[i] = input_samples[input_idx];
    }
    
    // Convert output samples back to bytes
    std::vector<uint8_t> output_data(output_samples.size() * sizeof(int16_t));
    std::memcpy(output_data.data(), output_samples.data(), output_data.size());
    
    LOG_INFO("Downsampling complete: {} bytes -> {} bytes", input_data.size(), output_data.size());
    return output_data;
}

bool savePCMToWav(const std::string& filename, const std::vector<uint8_t>& pcm_data, int sample_rate, int channels) {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file for writing: {}", filename);
            return false;
        }
        
        // Validate channels
        if (channels != 1 && channels != 2) {
            LOG_ERROR("Invalid number of channels: {}, must be 1 (mono) or 2 (stereo)", channels);
            return false;
        }
        
        // Prepare WAV header
        WavHeader header;
        header.num_channels = channels;
        header.sample_rate = sample_rate;
        header.bits_per_sample = 16; // 16-bit PCM
        header.block_align = header.num_channels * header.bits_per_sample / 8;
        header.byte_rate = header.sample_rate * header.block_align;
        header.data_size = pcm_data.size();
        header.riff_size = 36 + header.data_size; // 36 = size of header minus first 8 bytes
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Write PCM data
        file.write(reinterpret_cast<const char*>(pcm_data.data()), pcm_data.size());
        
        file.close();
        LOG_INFO("Saved PCM data to WAV file: {} ({} channels, {}Hz)", 
                 filename, channels, sample_rate);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving PCM to WAV: {}", e.what());
        return false;
    }
}

bool saveFloatToWav(const std::string& filename, const std::vector<float>& samples, int sample_rate, int channels) {
    try {
        // Validate channels
        if (channels != 1 && channels != 2) {
            LOG_ERROR("Invalid number of channels: {}, must be 1 (mono) or 2 (stereo)", channels);
            return false;
        }
        
        // Convert float samples to 16-bit PCM
        std::vector<int16_t> pcm_data(samples.size());
        for (size_t i = 0; i < samples.size(); i++) {
            // Clamp to [-1.0, 1.0] and convert to int16_t
            float sample = samples[i];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            pcm_data[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file for writing: {}", filename);
            return false;
        }
        
        // Prepare WAV header
        WavHeader header;
        header.num_channels = channels;
        header.sample_rate = sample_rate;
        header.bits_per_sample = 16; // 16-bit PCM
        header.block_align = header.num_channels * header.bits_per_sample / 8;
        header.byte_rate = header.sample_rate * header.block_align;
        header.data_size = pcm_data.size() * sizeof(int16_t);
        header.riff_size = 36 + header.data_size; // 36 = size of header minus first 8 bytes
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Write PCM data
        file.write(reinterpret_cast<const char*>(pcm_data.data()), pcm_data.size() * sizeof(int16_t));
        
        file.close();
        LOG_INFO("Saved float samples to WAV file: {} ({} channels, {}Hz)", 
                 filename, channels, sample_rate);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving float samples to WAV: {}", e.what());
        return false;
    }
}

} // namespace audio_utils 