#if defined(_WIN32)
#include "audio/WasapiCapture.h"
#include <wil/result.h>
#include <vector>

namespace recorder {

WasapiCapture::WasapiCapture() = default;

WasapiCapture::~WasapiCapture() {
    Stop();
    if (captureEvent_) {
        CloseHandle(captureEvent_);
        captureEvent_ = NULL;
    }
}

HRESULT WasapiCapture::Initialize(const std::wstring& deviceId, AudioBufferCallback callback) {
    deviceId_ = deviceId;
    callback_ = std::move(callback);

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)));

    Microsoft::WRL::ComPtr<IMMDevice> device;
    if (deviceId_.empty() || deviceId_ == L"default") {
        RETURN_IF_FAILED(enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &device));
    } else {
        RETURN_IF_FAILED(enumerator->GetDevice(deviceId_.c_str(), &device));
    }

    RETURN_IF_FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient_));

    WAVEFORMATEX* pWfx = nullptr;
    RETURN_IF_FAILED(audioClient_->GetMixFormat(&pWfx));
    waveFormat_ = *pWfx;
    CoTaskMemFree(pWfx);

    // Ensure 16-bit PCM format configuration if floating point mix format
    if (waveFormat_.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* pEx = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(&waveFormat_);
        if (pEx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            waveFormat_.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat_.wBitsPerSample = 16;
            waveFormat_.nBlockAlign = (waveFormat_.nChannels * waveFormat_.wBitsPerSample) / 8;
            waveFormat_.nAvgBytesPerSec = waveFormat_.nSamplesPerSec * waveFormat_.nBlockAlign;
        }
    }

    captureEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!captureEvent_) return HRESULT_FROM_WIN32(GetLastError());

    REFERENCE_TIME bufferDuration = 10000000; // 1 second buffer
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

    return S_OK;
}

HRESULT WasapiCapture::Start() {
    if (isCapturing_) return S_OK;
    if (!audioClient_ || !captureClient_) return E_UNEXPECTED;

    RETURN_IF_FAILED(audioClient_->Start());
    isCapturing_ = true;

    captureThread_ = std::jthread([this]() { CaptureLoop(); });
    return S_OK;
}

void WasapiCapture::Stop() {
    if (!isCapturing_) return;
    isCapturing_ = false;

    if (captureEvent_) {
        SetEvent(captureEvent_);
    }

    if (captureThread_.joinable()) {
        captureThread_.join();
    }

    if (audioClient_) {
        audioClient_->Stop();
    }
}

void WasapiCapture::CaptureLoop() {
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
                    // Convert float or 32-bit int to 16-bit PCM
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
