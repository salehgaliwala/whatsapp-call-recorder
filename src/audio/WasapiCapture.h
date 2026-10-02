#ifndef WASAPI_CAPTURE_H
#define WASAPI_CAPTURE_H

#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <string>

namespace recorder {

class WasapiCapture {
public:
    using AudioBufferCallback = std::function<void(const int16_t* pcmData, size_t sampleCount, int channels, int sampleRate)>;

    WasapiCapture();
    ~WasapiCapture();

    HRESULT Initialize(const std::wstring& deviceId = L"", AudioBufferCallback callback = nullptr);
    HRESULT Start();
    void Stop();

    WAVEFORMATEX GetFormat() const { return waveFormat_; }

private:
    void CaptureLoop();

    AudioBufferCallback callback_;
    std::wstring deviceId_;
    std::jthread captureThread_;
    std::atomic<bool> isCapturing_{false};
    HANDLE captureEvent_{NULL};

    Microsoft::WRL::ComPtr<IAudioClient> audioClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient_;
    WAVEFORMATEX waveFormat_{0};
};

} // namespace recorder

#endif // _WIN32
#endif // WASAPI_CAPTURE_H
