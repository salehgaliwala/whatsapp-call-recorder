#ifndef MEDIA_FOUNDATION_ENCODER_H
#define MEDIA_FOUNDATION_ENCODER_H

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <readwrite.h>
#include <wrl/client.h>
#include <string>
#include <cstdint>
#include <mutex>

namespace recorder {

class MediaFoundationEncoder {
public:
    MediaFoundationEncoder();
    ~MediaFoundationEncoder();

    HRESULT Initialize(const std::wstring& outputFilePath, int sampleRate = 48000, int channels = 2, int bitrateKbps = 160);
    HRESULT WritePCMFrame(const int16_t* pcmSamples, size_t sampleCount);
    HRESULT FinalizeAndClose();

private:
    Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter_;
    DWORD streamIndex_{0};
    LONGLONG sampleTimeNs_{0};
    int sampleRate_{48000};
    int channels_{2};
    std::mutex mutex_;
    bool isInitialized_{false};
};

} // namespace recorder

#endif // _WIN32
#endif // MEDIA_FOUNDATION_ENCODER_H
