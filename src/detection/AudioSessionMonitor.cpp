#if defined(_WIN32)
#include "detection/AudioSessionMonitor.h"
#include <tlhelp32.h>
#include <psapi.h>

namespace recorder {

AudioSessionMonitor::AudioSessionMonitor() = default;

AudioSessionMonitor::~AudioSessionMonitor() {
    Stop();
}

void AudioSessionMonitor::SetCallback(SessionStateCallback callback) {
    callback_ = std::move(callback);
}

HRESULT AudioSessionMonitor::Initialize(const std::wstring& targetProcessName) {
    targetProcessName_ = targetProcessName;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)));

    Microsoft::WRL::ComPtr<IMMDevice> device;
    RETURN_IF_FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));

    RETURN_IF_FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &sessionManager_));

    RETURN_IF_FAILED(sessionManager_->RegisterSessionNotification(this));

    // Check existing sessions
    Microsoft::WRL::ComPtr<IAudioSessionEnumerator> sessionEnum;
    if (SUCCEEDED(sessionManager_->GetSessionEnumerator(&sessionEnum))) {
        int count = 0;
        sessionEnum->GetCount(&count);
        for (int i = 0; i < count; ++i) {
            Microsoft::WRL::ComPtr<IAudioSessionControl> session;
            if (SUCCEEDED(sessionEnum->GetSession(i, &session))) {
                DWORD pid = GetProcessIdFromSession(session.Get());
                if (IsTargetProcess(pid)) {
                    AttachToSession(session.Get());
                    break;
                }
            }
        }
    }

    return S_OK;
}

void AudioSessionMonitor::Stop() {
    if (sessionManager_) {
        sessionManager_->UnregisterSessionNotification(this);
        sessionManager_ = nullptr;
    }
    if (activeSession_) {
        Microsoft::WRL::ComPtr<IAudioSessionControl2> session2;
        if (SUCCEEDED(activeSession_.As(&session2))) {
            session2->UnregisterAudioSessionNotification(this);
        }
        activeSession_ = nullptr;
    }
    isActive_ = false;
    targetPid_ = 0;
}

DWORD AudioSessionMonitor::GetProcessIdFromSession(IAudioSessionControl* session) {
    if (!session) return 0;
    Microsoft::WRL::ComPtr<IAudioSessionControl2> session2;
    if (SUCCEEDED(session->QueryInterface(IID_PPV_ARGS(&session2)))) {
        DWORD pid = 0;
        if (SUCCEEDED(session2->GetProcessId(&pid))) {
            return pid;
        }
    }
    return 0;
}

bool AudioSessionMonitor::IsTargetProcess(DWORD pid) {
    if (pid == 0) return false;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    wchar_t processName[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    bool isMatch = false;
    if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
        std::wstring exeName = processName;
        size_t lastSlash = exeName.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeName = exeName.substr(lastSlash + 1);
        }
        isMatch = (_wcsicmp(exeName.c_str(), targetProcessName_.c_str()) == 0);
    }
    CloseHandle(hProcess);
    return isMatch;
}

HRESULT AudioSessionMonitor::AttachToSession(IAudioSessionControl* session) {
    if (!session) return E_POINTER;
    activeSession_ = session;

    Microsoft::WRL::ComPtr<IAudioSessionControl2> session2;
    RETURN_IF_FAILED(activeSession_.As(&session2));

    DWORD pid = 0;
    session2->GetProcessId(&pid);
    targetPid_ = pid;

    RETURN_IF_FAILED(session2->RegisterAudioSessionNotification(this));

    AudioSessionState state;
    if (SUCCEEDED(session2->GetState(&state))) {
        OnStateChanged(state);
    }

    return S_OK;
}

IFACEMETHODIMP AudioSessionMonitor::OnSessionCreated(IAudioSessionControl* NewSession) {
    DWORD pid = GetProcessIdFromSession(NewSession);
    if (IsTargetProcess(pid)) {
        AttachToSession(NewSession);
    }
    return S_OK;
}

IFACEMETHODIMP AudioSessionMonitor::OnStateChanged(AudioSessionState NewState) {
    bool active = (NewState == AudioSessionStateActive);
    isActive_ = active;

    if (callback_) {
        callback_(active, targetPid_.load());
    }
    return S_OK;
}

IFACEMETHODIMP AudioSessionMonitor::OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) {
    isActive_ = false;
    if (callback_) {
        callback_(false, targetPid_.load());
    }
    return S_OK;
}

} // namespace recorder
#endif // _WIN32
