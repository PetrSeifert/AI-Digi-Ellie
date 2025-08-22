#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace audio_utils {

/**
 * Save PCM audio data to a WAV file
 * 
 * @param filename Output WAV filename
 * @param pcm_data Raw PCM audio data (16-bit, mono or stereo)
 * @param sample_rate Sample rate in Hz (default: 16000)
 * @param channels Number of channels (1=mono, 2=stereo) (default: 2)
 * @return true if successful, false otherwise
 */
bool savePCMToWav(const std::string& filename, const std::vector<uint8_t>& pcm_data, int sample_rate = 16000, int channels = 2);

/**
 * Save float audio samples to a WAV file
 * 
 * @param filename Output WAV filename
 * @param samples Float audio samples in range [-1.0, 1.0]
 * @param sample_rate Sample rate in Hz (default: 16000)
 * @param channels Number of channels (1=mono, 2=stereo) (default: 1)
 * @return true if successful, false otherwise
 */
bool saveFloatToWav(const std::string& filename, const std::vector<float>& samples, int sample_rate = 16000, int channels = 1);

/**
 * Downsample 16-bit PCM audio from one sample rate to another
 * 
 * @param input_data Input PCM audio data
 * @param input_sample_rate Input sample rate in Hz
 * @param output_sample_rate Output sample rate in Hz
 * @return Downsampled PCM audio data
 */
std::vector<uint8_t> downsamplePCM(const std::vector<uint8_t>& input_data, int input_sample_rate, int output_sample_rate);

/**
 * Convert stereo PCM audio to mono
 * 
 * @param stereo_data Input stereo PCM audio data (interleaved left-right channels)
 * @return Mono PCM audio data
 */
std::vector<uint8_t> stereoToMono(const std::vector<uint8_t>& stereo_data);

} // namespace audio_utils 