#if defined(_WIN32)
#include "audio/ProcessLoopbackCapture.h"
#include <wil/result.h>
#include <algorithm>

namespace recorder {

ProcessLoopbackCapture::ProcessLoopbackCapture() {
    captureEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    activateEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

ProcessLoopbackCapture::~ProcessLoopbackCapture() {
    StopCapture();
    if (captureEvent_) CloseHandle(captureEvent_);
    if (activateEvent_) CloseHandle(activateEvent_);
}

HRESULT ProcessLoopbackCapture::StartCapture(DWORD processId, AudioBufferCallback callback) {
    if (isCapturing_) return S_OK;
    targetPid_ = processId;
    callback_ = std::move(callback);

    AUDIOCLIENT_ACTIVATION_PARAMS AUDIOCLIENT_ACTIVATION_PARAMS_LOOPBACK = {};
    AUDIOCLIENT_ACTIVATION_PARAMS_LOOPBACK.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    AUDIOCLIENT_ACTIVATION_PARAMS_LOOPBACK.ProcessLoopbackParams.TargetProcessId = targetPid_;
    AUDIOCLIENT_ACTIVATION_PARAMS_LOOPBACK.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_PROCESS_TREE;

    PROPVARIANT activateParams = {};
    activateParams.vt = VT_BLOB;
    activateParams.blob.cbSize = sizeof(AUDIOCLIENT_ACTIVATION_PARAMS);
    activateParams.blob.pBlobData = reinterpret_cast<BYTE*>(&AUDIOCLIENT_ACTIVATION_PARAMS_LOOPBACK);

    Microsoft::WRL::ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
    ResetEvent(activateEvent_);

    HRESULT hr = ActivateAudioInterfaceAsync(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
        __uuidof(IAudioClient),
        &activateParams,
        this,
        &asyncOp
    );

    if (FAILED(hr)) return hr;

    // Wait for activation completion
    WaitForSingleObject(activateEvent_, 3000);
    if (FAILED(activationResult_)) return activationResult_;

    WAVEFORMATEX* pWfx = nullptr;
    RETURN_IF_FAILED(audioClient_->GetMixFormat(&pWfx));
    waveFormat_ = *pWfx;
    CoTaskMemFree(pWfx);

    REFERENCE_TIME bufferDuration = 10000000; // 1 second
    // Process-scoped loopback must NOT use AUDCLNT_STREAMFLAGS_LOOPBACK flag during IAudioClient::Initialize
    RETURN_IF_FAILED(audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        &waveFormat_,
        nullptr
    ));

    RETURN_IF_FAILED(audioClient_->SetEventHandle(captureEvent_));
    RETURN_IF_FAILED(audioClient_->GetService(IID_PPV_ARGS(&captureClient_)));

    RETURN_IF_FAILED(audioClient_->Start());
    isCapturing_ = true;

    captureThread_ = std::jthread([this]() { CaptureLoop(); });
    return S_OK;
}

IFACEMETHODIMP ProcessLoopbackCapture::ActivateCompleted(IActivateAudioInterfaceAsyncOperation* activateOperation) {
    HRESULT hrResult = E_FAIL;
    Microsoft::WRL::ComPtr<IUnknown> unkDevice;
    HRESULT hr = activateOperation->GetActivateResult(&hrResult, &unkDevice);

    if (SUCCEEDED(hr) && SUCCEEDED(hrResult)) {
        unkDevice.As(&audioClient_);
        activationResult_ = S_OK;
    } else {
        activationResult_ = FAILED(hr) ? hr : hrResult;
    }

    SetEvent(activateEvent_);
    return S_OK;
}

void ProcessLoopbackCapture::StopCapture() {
    if (!isCapturing_) return;
    isCapturing_ = false;

    if (captureEvent_) SetEvent(captureEvent_);
    if (captureThread_.joinable()) captureThread_.join();
    if (audioClient_) audioClient_->Stop();
}

void ProcessLoopbackCapture::CaptureLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (isCapturing_) {
        DWORD waitResult = WaitForSingleObject(captureEvent_, 100);
        if (waitResult != WAIT_OBJECT_0) continue;
        if (!isCapturing_) break;

        UINT32 packetLength = 0;
        HRESULT hr = captureClient_->GetNextPacketSize(&packetLength);
        while (SUCCEEDED(hr) && packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;

            hr = captureClient_->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            if (numFramesAvailable > 0 && callback_) {
                size_t totalSamples = numFramesAvailable * waveFormat_.nChannels;
                std::vector<int16_t> pcmSamples(totalSamples);

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(pcmSamples.begin(), pcmSamples.end(), static_cast<int16_t>(0));
                } else if (waveFormat_.wBitsPerSample == 16) {
                    const int16_t* src = reinterpret_cast<const int16_t*>(pData);
                    std::copy_n(src, totalSamples, pcmSamples.begin());
                } else if (waveFormat_.wBitsPerSample == 32) {
                    const float* src = reinterpret_cast<const float*>(pData);
                    for (size_t i = 0; i < totalSamples; ++i) {
                        float s = src[i];
                        if (s > 1.0f) s = 1.0f;
                        if (s < -1.0f) s = -1.0f;
                        pcmSamples[i] = static_cast<int16_t>(s * 32767.0f);
                    }
                }

                callback_(pcmSamples.data(), pcmSamples.size(), waveFormat_.nChannels, waveFormat_.nSamplesPerSec);
            }

            captureClient_->ReleaseBuffer(numFramesAvailable);
            hr = captureClient_->GetNextPacketSize(&packetLength);
        }
    }

    CoUninitialize();
}

} // namespace recorder
#endif // _WIN32
