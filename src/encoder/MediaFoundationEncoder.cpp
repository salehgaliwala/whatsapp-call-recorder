#if defined(_WIN32)
#include "encoder/MediaFoundationEncoder.h"
#include <wil/result.h>

namespace recorder {

MediaFoundationEncoder::MediaFoundationEncoder() {
    MFStartup(MF_VERSION);
}

MediaFoundationEncoder::~MediaFoundationEncoder() {
    FinalizeAndClose();
    MFShutdown();
}

HRESULT MediaFoundationEncoder::Initialize(const std::wstring& outputFilePath, int sampleRate, int channels, int bitrateKbps) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isInitialized_) return S_OK;

    sampleRate_ = sampleRate;
    channels_ = channels;
    sampleTimeNs_ = 0;

    Microsoft::WRL::ComPtr<IMFAttributes> attributes;
    RETURN_IF_FAILED(MFCreateAttributes(&attributes, 1));
    RETURN_IF_FAILED(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));

    RETURN_IF_FAILED(MFCreateSinkWriterFromURL(outputFilePath.c_str(), nullptr, attributes.Get(), &sinkWriter_));

    // Output Media Type (M4A AAC)
    Microsoft::WRL::ComPtr<IMFMediaType> mediaTypeOut;
    RETURN_IF_FAILED(MFCreateMediaType(&mediaTypeOut));
    RETURN_IF_FAILED(mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
    RETURN_IF_FAILED(mediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
    RETURN_IF_FAILED(mediaTypeOut->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels_));
    RETURN_IF_FAILED(mediaTypeOut->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate_));
    RETURN_IF_FAILED(mediaTypeOut->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (bitrateKbps * 1000) / 8));
    RETURN_IF_FAILED(mediaTypeOut->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));

    RETURN_IF_FAILED(sinkWriter_->AddStream(mediaTypeOut.Get(), &streamIndex_));

    // Input Media Type (PCM Uncompressed)
    Microsoft::WRL::ComPtr<IMFMediaType> mediaTypeIn;
    RETURN_IF_FAILED(MFCreateMediaType(&mediaTypeIn));
    RETURN_IF_FAILED(mediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
    RETURN_IF_FAILED(mediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
    RETURN_IF_FAILED(mediaTypeIn->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels_));
    RETURN_IF_FAILED(mediaTypeIn->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate_));
    RETURN_IF_FAILED(mediaTypeIn->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
    RETURN_IF_FAILED(mediaTypeIn->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, (channels_ * 16) / 8));
    RETURN_IF_FAILED(mediaTypeIn->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sampleRate_ * ((channels_ * 16) / 8)));

    RETURN_IF_FAILED(sinkWriter_->SetInputMediaType(streamIndex_, mediaTypeIn.Get(), nullptr));
    RETURN_IF_FAILED(sinkWriter_->BeginWriting());

    isInitialized_ = true;
    return S_OK;
}

HRESULT MediaFoundationEncoder::WritePCMFrame(const int16_t* pcmSamples, size_t sampleCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_ || !sinkWriter_ || sampleCount == 0) return E_UNEXPECTED;

    DWORD byteCount = static_cast<DWORD>(sampleCount * sizeof(int16_t));

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    RETURN_IF_FAILED(MFCreateMemoryBuffer(byteCount, &buffer));

    BYTE* pData = nullptr;
    RETURN_IF_FAILED(buffer->Lock(&pData, nullptr, nullptr));
    std::copy_n(reinterpret_cast<const BYTE*>(pcmSamples), byteCount, pData);
    RETURN_IF_FAILED(buffer->Unlock());
    RETURN_IF_FAILED(buffer->SetCurrentLength(byteCount));

    Microsoft::WRL::ComPtr<IMFSample> sample;
    RETURN_IF_FAILED(MFCreateSample(&sample));
    RETURN_IF_FAILED(sample->AddBuffer(buffer.Get()));

    // 100-nanosecond units calculation
    size_t frameCount = sampleCount / channels_;
    LONGLONG durationNs = (static_cast<LONGLONG>(frameCount) * 10000000LL) / sampleRate_;

    RETURN_IF_FAILED(sample->SetSampleTime(sampleTimeNs_));
    RETURN_IF_FAILED(sample->SetSampleDuration(durationNs));

    RETURN_IF_FAILED(sinkWriter_->WriteSample(streamIndex_, sample.Get()));
    sampleTimeNs_ += durationNs;

    return S_OK;
}

HRESULT MediaFoundationEncoder::FinalizeAndClose() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) return S_OK;

    if (sinkWriter_) {
        sinkWriter_->Finalize();
        sinkWriter_ = nullptr;
    }

    isInitialized_ = false;
    return S_OK;
}

} // namespace recorder
#endif // _WIN32
