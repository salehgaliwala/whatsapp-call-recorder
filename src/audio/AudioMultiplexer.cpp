#include "audio/AudioMultiplexer.h"
#include <algorithm>

namespace recorder {

std::vector<int16_t> AudioMultiplexer::Multiplex(
    const int16_t* micSamples, size_t micSampleCount, int micChannels,
    const int16_t* remoteSamples, size_t remoteSampleCount, int remoteChannels) {

    size_t micFrames = (micChannels > 0) ? (micSampleCount / micChannels) : 0;
    size_t remoteFrames = (remoteChannels > 0) ? (remoteSampleCount / remoteChannels) : 0;
    size_t totalFrames = std::max(micFrames, remoteFrames);

    std::vector<int16_t> stereoOutput(totalFrames * 2, 0);

    for (size_t i = 0; i < totalFrames; ++i) {
        int16_t left = 0;
        int16_t right = 0;

        if (i < micFrames) {
            if (micChannels == 1) {
                left = micSamples[i];
            } else if (micChannels >= 2) {
                int32_t sum = static_cast<int32_t>(micSamples[i * micChannels]) +
                              static_cast<int32_t>(micSamples[i * micChannels + 1]);
                left = static_cast<int16_t>(sum / 2);
            }
        }

        if (i < remoteFrames) {
            if (remoteChannels == 1) {
                right = remoteSamples[i];
            } else if (remoteChannels >= 2) {
                int32_t sum = static_cast<int32_t>(remoteSamples[i * remoteChannels]) +
                              static_cast<int32_t>(remoteSamples[i * remoteChannels + 1]);
                right = static_cast<int16_t>(sum / 2);
            }
        }

        stereoOutput[i * 2] = left;      // Channel 0 (Left)
        stereoOutput[i * 2 + 1] = right;  // Channel 1 (Right)
    }

    return stereoOutput;
}

} // namespace recorder
