#ifndef AUDIO_MULTIPLEXER_H
#define AUDIO_MULTIPLEXER_H

#include <vector>
#include <cstdint>
#include <cstddef>

namespace recorder {

class AudioMultiplexer {
public:
    // Takes local microphone samples (mono or stereo) and remote loopback samples (mono or stereo)
    // and multiplexes them into 2-channel stereo 16-bit PCM:
    // Channel 0 (Left) = Local Mic (Outgoing)
    // Channel 1 (Right) = Remote Process Loopback (Incoming)
    static std::vector<int16_t> Multiplex(
        const int16_t* micSamples, size_t micSampleCount, int micChannels,
        const int16_t* remoteSamples, size_t remoteSampleCount, int remoteChannels
    );
};

} // namespace recorder

#endif // AUDIO_MULTIPLEXER_H
