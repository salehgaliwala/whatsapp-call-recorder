#ifndef RECORD_TYPES_H
#define RECORD_TYPES_H

#include <string>
#include <cstdint>
#include <chrono>

namespace recorder {

enum class CallState {
    Idle,
    CallDetected,
    Recording,
    CallEnded,
    FlushingEncoder
};

inline const char* CallStateToString(CallState state) {
    switch (state) {
        case CallState::Idle: return "Idle";
        case CallState::CallDetected: return "CallDetected";
        case CallState::Recording: return "Recording";
        case CallState::CallEnded: return "CallEnded";
        case CallState::FlushingEncoder: return "FlushingEncoder";
        default: return "Unknown";
    }
}

struct CallRecord {
    int64_t callId{-1};
    std::string contactOrNumber;
    std::string startTime;
    std::string endTime;
    double durationSeconds{0.0};
    std::string filePath;
};

struct AppConfig {
    std::string outputDirectory;
    std::string preferredMicDeviceId;
    double debounceTimeSeconds{2.5};
    int targetSampleRate{48000};
    int targetChannels{2}; // L: Mic, R: Remote WhatsApp
    int targetBitDepth{16};
    int aacBitrateKbps{160};
};

} // namespace recorder

#endif // RECORD_TYPES_H
