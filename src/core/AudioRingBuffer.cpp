#include "core/AudioRingBuffer.h"

namespace recorder {

AudioRingBuffer::AudioRingBuffer(size_t capacityInSamples)
    : buffer_(capacityInSamples + 1), capacity_(capacityInSamples + 1) {}

size_t AudioRingBuffer::AvailableRead() const {
    size_t writePos = writeHead_.load(std::memory_order_acquire);
    size_t readPos = readHead_.load(std::memory_order_relaxed);
    if (writePos >= readPos) {
        return writePos - readPos;
    }
    return capacity_ - (readPos - writePos);
}

size_t AudioRingBuffer::AvailableWrite() const {
    return capacity_ - 1 - AvailableRead();
}

size_t AudioRingBuffer::Write(const int16_t* data, size_t sampleCount) {
    size_t toWrite = std::min(sampleCount, AvailableWrite());
    if (toWrite == 0) return 0;

    size_t writePos = writeHead_.load(std::memory_order_relaxed);
    size_t firstPart = std::min(toWrite, capacity_ - writePos);
    size_t secondPart = toWrite - firstPart;

    std::copy_n(data, firstPart, buffer_.data() + writePos);
    if (secondPart > 0) {
        std::copy_n(data + firstPart, secondPart, buffer_.data());
    }

    size_t newWritePos = (writePos + toWrite) % capacity_;
    writeHead_.store(newWritePos, std::memory_order_release);

    return toWrite;
}

size_t AudioRingBuffer::Read(int16_t* destination, size_t sampleCount) {
    size_t available = AvailableRead();
    size_t toRead = std::min(sampleCount, available);
    if (toRead == 0) return 0;

    size_t readPos = readHead_.load(std::memory_order_relaxed);
    size_t firstPart = std::min(toRead, capacity_ - readPos);
    size_t secondPart = toRead - firstPart;

    std::copy_n(buffer_.data() + readPos, firstPart, destination);
    if (secondPart > 0) {
        std::copy_n(buffer_.data(), secondPart, destination + firstPart);
    }

    size_t newReadPos = (readPos + toRead) % capacity_;
    readHead_.store(newReadPos, std::memory_order_release);

    return toRead;
}

void AudioRingBuffer::Clear() {
    writeHead_.store(0, std::memory_order_relaxed);
    readHead_.store(0, std::memory_order_relaxed);
}

} // namespace recorder
