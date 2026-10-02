#ifndef TRAY_APP_H
#define TRAY_APP_H

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include "detection/CallStateMachine.h"
#include "detection/AudioSessionMonitor.h"
#include "detection/UIAutomationHook.h"
#include "audio/WasapiCapture.h"
#include "audio/ProcessLoopbackCapture.h"
#include "audio/DeviceChangeMonitor.h"
#include "encoder/MediaFoundationEncoder.h"
#include "db/SqliteLogger.h"
#include "core/AudioRingBuffer.h"
#include "audio/AudioResampler.h"
#include <memory>
#include <thread>
#include <atomic>

namespace recorder {

class TrayApp {
public:
    TrayApp();
    ~TrayApp();

    bool Initialize(HINSTANCE hInstance);
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowContextMenu();
    void ShowToastNotification(const std::wstring& title, const std::wstring& message);

    void OnStateChanged(CallState oldState, CallState newState);
    void StartRecordingSession();
    void StopRecordingSession();
    void AudioProcessingLoop();

    HINSTANCE hInstance_{NULL};
    HWND hwnd_{NULL};
    NOTIFYICONDATAW nid_{0};

    CallStateMachine stateMachine_{2.5};
    Microsoft::WRL::ComPtr<AudioSessionMonitor> sessionMonitor_;
    UIAutomationHook uiHook_;
    SqliteLogger dbLogger_;

    std::unique_ptr<WasapiCapture> micCapture_;
    Microsoft::WRL::ComPtr<ProcessLoopbackCapture> loopbackCapture_;
    Microsoft::WRL::ComPtr<DeviceChangeMonitor> deviceMonitor_;
    MediaFoundationEncoder encoder_;

    AudioRingBuffer micRingBuffer_{48000 * 2 * 5};
    AudioRingBuffer loopbackRingBuffer_{48000 * 2 * 5};

    std::unique_ptr<AudioResampler> micResampler_;
    std::unique_ptr<AudioResampler> loopbackResampler_;

    std::jthread processingThread_;
    std::jthread monitoringThread_;
    std::atomic<bool> isRecording_{false};
    std::atomic<bool> isRunning_{false};

    int64_t currentCallDbId_{-1};
    std::string currentFilePath_;
    std::chrono::steady_clock::time_point recordingStartTime_;
};

} // namespace recorder

#endif // _WIN32
#endif // TRAY_APP_H
