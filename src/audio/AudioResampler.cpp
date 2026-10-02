#include "audio/AudioResampler.h"
#include <algorithm>
#include <cmath>

namespace recorder {

AudioResampler::AudioResampler(int sourceSampleRate, int targetSampleRate, int channels)
    : sourceRate_(sourceSampleRate), targetRate_(targetSampleRate), channels_(channels) {
    lastFrame_.resize(channels_, 0);
}

void AudioResampler::Reset() {
    phase_ = 0.0;
    std::fill(lastFrame_.begin(), lastFrame_.end(), static_cast<int16_t>(0));
}

std::vector<int16_t> AudioResampler::Resample(const int16_t* inputSamples, size_t inputSampleCount) {
    if (sourceRate_ == targetRate_ || inputSampleCount == 0) {
        return std::vector<int16_t>(inputSamples, inputSamples + inputSampleCount);
    }

    size_t inputFrames = inputSampleCount / channels_;
    if (inputFrames == 0) return {};

    double ratio = static_cast<double>(sourceRate_) / static_cast<double>(targetRate_);
    size_t estimatedOutputFrames = static_cast<size_t>(std::ceil(inputFrames / ratio)) + 2;
    std::vector<int16_t> output;
    output.reserve(estimatedOutputFrames * channels_);

    while (phase_ < inputFrames) {
        size_t idx0 = static_cast<size_t>(std::floor(phase_));
        double frac = phase_ - idx0;
        size_t idx1 = idx0 + 1;

        for (int c = 0; c < channels_; ++c) {
            double sample0 = (idx0 == 0) ? lastFrame_[c] : inputSamples[(idx0 - 1) * channels_ + c];
            double sample1 = inputSamples[idx0 * channels_ + c];
            if (idx1 < inputFrames) {
                // Linear interpolation between sample1 and next sample
                double sample2 = inputSamples[idx1 * channels_ + c];
                double val = sample1 + frac * (sample2 - sample1);
                output.push_back(static_cast<int16_t>(std::clamp(val, -32768.0, 32767.0)));
            } else {
                double val = sample0 + frac * (sample1 - sample0);
                output.push_back(static_cast<int16_t>(std::clamp(val, -32768.0, 32767.0)));
            }
        }

        phase_ += ratio;
    }

    phase_ -= inputFrames;
    if (inputFrames > 0) {
        for (int c = 0; c < channels_; ++c) {
            lastFrame_[c] = inputSamples[(inputFrames - 1) * channels_ + c];
        }
    }

    return output;
}

} // namespace recorder
