#ifndef AUDIO_RING_BUFFER_H
#define AUDIO_RING_BUFFER_H

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace recorder {

// Thread-safe lock-free SPCM ring buffer for 16-bit PCM stereo audio samples
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacityInSamples = 48000 * 2 * 10); // Default 10 sec @ 48kHz stereo
    ~AudioRingBuffer() = default;

    // Non-copyable
    AudioRingBuffer(const AudioRingBuffer&) = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;

    size_t Write(const int16_t* data, size_t sampleCount);
    size_t Read(int16_t* destination, size_t sampleCount);

    size_t AvailableRead() const;
    size_t AvailableWrite() const;

    void Clear();

private:
    std::vector<int16_t> buffer_;
    size_t capacity_;
    alignas(64) std::atomic<size_t> writeHead_{0};
    alignas(64) std::atomic<size_t> readHead_{0};
};

} // namespace recorder

#endif // AUDIO_RING_BUFFER_H
