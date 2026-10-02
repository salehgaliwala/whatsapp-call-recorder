#ifndef PROCESS_LOOPBACK_CAPTURE_H
#define PROCESS_LOOPBACK_CAPTURE_H

#if defined(_WIN32)
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>

namespace recorder {

class ProcessLoopbackCapture :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IActivateAudioInterfaceCompletionHandler
    >
{
public:
    using AudioBufferCallback = std::function<void(const int16_t* pcmData, size_t sampleCount, int channels, int sampleRate)>;

    ProcessLoopbackCapture();
    ~ProcessLoopbackCapture();

    HRESULT StartCapture(DWORD processId, AudioBufferCallback callback);
    void StopCapture();

    // IActivateAudioInterfaceCompletionHandler
    IFACEMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* activateOperation) override;

    WAVEFORMATEX GetFormat() const { return waveFormat_; }

private:
    void CaptureLoop();

    DWORD targetPid_{0};
    AudioBufferCallback callback_;
    std::jthread captureThread_;
    std::atomic<bool> isCapturing_{false};
    HANDLE captureEvent_{NULL};
    HANDLE activateEvent_{NULL};

    Microsoft::WRL::ComPtr<IAudioClient> audioClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient_;
    WAVEFORMATEX waveFormat_{0};
    HRESULT activationResult_{E_FAIL};
};

} // namespace recorder

#endif // _WIN32
#endif // PROCESS_LOOPBACK_CAPTURE_H
