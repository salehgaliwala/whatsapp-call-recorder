#ifndef AUDIO_RESAMPLER_H
#define AUDIO_RESAMPLER_H

#include <vector>
#include <cstdint>
#include <cstddef>

namespace recorder {

class AudioResampler {
public:
    AudioResampler(int sourceSampleRate, int targetSampleRate, int channels);
    ~AudioResampler() = default;

    // Resample 16-bit PCM buffer. Returns number of output samples (total across all channels).
    std::vector<int16_t> Resample(const int16_t* inputSamples, size_t inputSampleCount);

    void Reset();

private:
    int sourceRate_;
    int targetRate_;
    int channels_;
    double phase_{0.0};
    std::vector<int16_t> lastFrame_;
};

} // namespace recorder

#endif // AUDIO_RESAMPLER_H
