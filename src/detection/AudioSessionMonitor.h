#ifndef AUDIO_SESSION_MONITOR_H
#define AUDIO_SESSION_MONITOR_H

#if defined(_WIN32)
#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wil/result.h>
#include <string>
#include <atomic>
#include <functional>

namespace recorder {

class AudioSessionMonitor :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IAudioSessionNotification,
        IAudioSessionEvents
    >
{
public:
    using SessionStateCallback = std::function<void(bool active, DWORD processId)>;

    AudioSessionMonitor();
    ~AudioSessionMonitor();

    HRESULT Initialize(const std::wstring& targetProcessName = L"WhatsApp.exe");
    void SetCallback(SessionStateCallback callback);
    void Stop();

    DWORD GetActiveProcessId() const { return targetPid_.load(); }
    bool IsSessionActive() const { return isActive_.load(); }

    // IAudioSessionNotification
    IFACEMETHODIMP OnSessionCreated(IAudioSessionControl* NewSession) override;

    // IAudioSessionEvents
    IFACEMETHODIMP OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) override { return S_OK; }
    IFACEMETHODIMP OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) override { return S_OK; }
    IFACEMETHODIMP OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext) override { return S_OK; }
    IFACEMETHODIMP OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel, LPCGUID EventContext) override { return S_OK; }
    IFACEMETHODIMP OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) override { return S_OK; }
    IFACEMETHODIMP OnStateChanged(AudioSessionState NewState) override;
    IFACEMETHODIMP OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override;

private:
    HRESULT AttachToSession(IAudioSessionControl* session);
    DWORD GetProcessIdFromSession(IAudioSessionControl* session);
    bool IsTargetProcess(DWORD pid);

    std::wstring targetProcessName_;
    SessionStateCallback callback_;
    std::atomic<DWORD> targetPid_{0};
    std::atomic<bool> isActive_{false};

    Microsoft::WRL::ComPtr<IAudioSessionManager2> sessionManager_;
    Microsoft::WRL::ComPtr<IAudioSessionControl> activeSession_;
};

} // namespace recorder

#endif // _WIN32
#endif // AUDIO_SESSION_MONITOR_H
