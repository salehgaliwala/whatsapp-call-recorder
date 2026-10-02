#if defined(_WIN32)
#include "ui/TrayApp.h"
#include "ui/SettingsDialog.h"
#include "core/Config.h"
#include "audio/AudioMultiplexer.h"
#include <shlobj.h>
#include <iomanip>
#include <sstream>
#include <filesystem>

#define WM_TRAYICON (WM_USER + 1)
#define IDM_OPEN_FOLDER 2001
#define IDM_SETTINGS    2002
#define IDM_STATUS      2003
#define IDM_EXIT        2004

namespace recorder {

namespace fs = std::filesystem;

static std::wstring Utf8ToUtf16(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

TrayApp::TrayApp() = default;

TrayApp::~TrayApp() {
    isRunning_ = false;
    RemoveTrayIcon();
}

bool TrayApp::Initialize(HINSTANCE hInstance) {
    hInstance_ = hInstance;

    // Register Window Class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = TrayApp::WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WhatsAppAutoCallRecorderClass";
    RegisterClassExW(&wc);

    // Create windowless background window
    hwnd_ = CreateWindowExW(
        0, wc.lpszClassName, L"WhatsApp Auto Call Recorder",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, this
    );

    if (!hwnd_) return false;

    // Initialize DB Logger
    std::string dbPath = (fs::path(Config::Instance().GetDefaultOutputDir()) / "recordings.db").string();
    fs::create_directories(Config::Instance().GetDefaultOutputDir());
    dbLogger_.Initialize(dbPath);

    // Initialize UI Automation
    uiHook_.Initialize();

    // Initialize State Machine Callback
    stateMachine_.SetStateChangedCallback([this](CallState oldState, CallState newState) {
        OnStateChanged(oldState, newState);
    });

    // Initialize Session Monitor
    sessionMonitor_ = Microsoft::WRL::Make<AudioSessionMonitor>();
    sessionMonitor_->SetCallback([this](bool active, DWORD pid) {
        stateMachine_.OnAudioSessionActive(active);
    });
    sessionMonitor_->Initialize(L"WhatsApp.exe");

    // Initialize Device Change Monitor
    deviceMonitor_ = Microsoft::WRL::Make<DeviceChangeMonitor>();
    deviceMonitor_->Initialize([this](EDataFlow flow, ERole role, LPCWSTR deviceId) {
        if (flow == eCapture && role == eCommunications) {
            if (isRecording_ && micCapture_) {
                micCapture_->Stop();
                micCapture_->Initialize(deviceId ? deviceId : L"", [this](const int16_t* pcm, size_t count, int ch, int sr) {
                    if (sr != 48000 && (!micResampler_)) {
                        micResampler_ = std::make_unique<AudioResampler>(sr, 48000, ch);
                    }
                    if (micResampler_) {
                        auto resampled = micResampler_->Resample(pcm, count);
                        micRingBuffer_.Write(resampled.data(), resampled.size());
                    } else {
                        micRingBuffer_.Write(pcm, count);
                    }
                });
                micCapture_->Start();
            }
        }
    });

    AddTrayIcon();

    // Monitoring thread loop (ticks UI automation + state machine)
    isRunning_ = true;
    monitoringThread_ = std::jthread([this]() {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);

        while (isRunning_) {
            DWORD pid = sessionMonitor_->GetActiveProcessId();
            if (pid > 0) {
                std::string contactName;
                bool uiPresent = uiHook_.IsCallUIPresent(pid, contactName);
                stateMachine_.OnCallUIPresent(uiPresent, contactName);
            } else {
                stateMachine_.OnCallUIPresent(false, "");
            }

            stateMachine_.Tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        CoUninitialize();
    });

    return true;
}

void TrayApp::AddTrayIcon() {
    ZeroMemory(&nid_, sizeof(NOTIFYICONDATAW));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"WhatsApp Auto Call Recorder (Idle)");

    Shell_NotifyIconW(NIM_ADD, &nid_);
}

void TrayApp::RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void TrayApp::ShowToastNotification(const std::wstring& title, const std::wstring& message) {
    nid_.uFlags |= NIF_INFO;
    wcscpy_s(nid_.szInfoTitle, title.c_str());
    wcscpy_s(nid_.szInfo, message.c_str());
    nid_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

LRESULT CALLBACK TrayApp::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TrayApp* pApp = nullptr;
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pApp = reinterpret_cast<TrayApp*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
    } else {
        pApp = reinterpret_cast<TrayApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pApp) {
        return pApp->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT TrayApp::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowContextMenu();
            }
            return 0;

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDM_OPEN_FOLDER) {
                std::string dir = Config::Instance().Get().outputDirectory;
                ShellExecuteA(NULL, "open", dir.c_str(), NULL, NULL, SW_SHOW);
            } else if (id == IDM_SETTINGS) {
                SettingsDialog::Show(hwnd_);
            } else if (id == IDM_EXIT) {
                PostQuitMessage(0);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void TrayApp::ShowContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_OPEN_FOLDER, L"Open Recordings Folder");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, IDM_SETTINGS, L"Settings / Device Select");

    std::wstring statusStr = isRecording_ ? L"Status: Recording Active" : L"Status: Idle (Monitoring)";
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING | MF_DISABLED, IDM_STATUS, statusStr.c_str());
    InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, NULL);
    DestroyMenu(hMenu);
}

void TrayApp::OnStateChanged(CallState oldState, CallState newState) {
    if (newState == CallState::Recording) {
        wcscpy_s(nid_.szTip, L"WhatsApp Auto Call Recorder (Recording)");
        Shell_NotifyIconW(NIM_MODIFY, &nid_);
        StartRecordingSession();
    } else if (newState == CallState::CallEnded) {
        wcscpy_s(nid_.szTip, L"WhatsApp Auto Call Recorder (Idle)");
        Shell_NotifyIconW(NIM_MODIFY, &nid_);
        StopRecordingSession();
    }
}

void TrayApp::StartRecordingSession() {
    if (isRecording_) return;

    std::string contact = stateMachine_.GetContactName();
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &time_t_now);

    std::ostringstream ss;
    ss << "WhatsApp_Call_" << contact << "_"
       << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S") << ".m4a";

    fs::path outDir(Config::Instance().Get().outputDirectory);
    fs::create_directories(outDir);
    fs::path fullPath = outDir / ss.str();
    currentFilePath_ = fullPath.string();

    // Log in DB
    currentCallDbId_ = dbLogger_.LogCallStart(contact, currentFilePath_);
    recordingStartTime_ = std::chrono::steady_clock::now();

    // Initialize Encoder
    encoder_.Initialize(fullPath.wstring(), 48000, 2, Config::Instance().Get().aacBitrateKbps);

    // Clear ring buffers
    micRingBuffer_.Clear();
    loopbackRingBuffer_.Clear();

    // Start Local Mic Capture
    micCapture_ = std::make_unique<WasapiCapture>();
    micCapture_->Initialize(L"", [this](const int16_t* pcm, size_t count, int ch, int sr) {
        if (sr != 48000 && (!micResampler_)) {
            micResampler_ = std::make_unique<AudioResampler>(sr, 48000, ch);
        }
        if (micResampler_) {
            auto resample = micResampler_->Resample(pcm, count);
            micRingBuffer_.Write(resample.data(), resample.size());
        } else {
            micRingBuffer_.Write(pcm, count);
        }
    });
    micCapture_->Start();

    // Start Process Loopback Capture
    DWORD pid = sessionMonitor_->GetActiveProcessId();
    loopbackCapture_ = Microsoft::WRL::Make<ProcessLoopbackCapture>();
    loopbackCapture_->StartCapture(pid, [this](const int16_t* pcm, size_t count, int ch, int sr) {
        if (sr != 48000 && (!loopbackResampler_)) {
            loopbackResampler_ = std::make_unique<AudioResampler>(sr, 48000, ch);
        }
        if (loopbackResampler_) {
            auto resample = loopbackResampler_->Resample(pcm, count);
            loopbackRingBuffer_.Write(resample.data(), resample.size());
        } else {
            loopbackRingBuffer_.Write(pcm, count);
        }
    });

    isRecording_ = true;
    processingThread_ = std::jthread([this]() { AudioProcessingLoop(); });

    ShowToastNotification(L"WhatsApp Call Recording Started", Utf8ToUtf16("Recording call with " + contact));
}

void TrayApp::StopRecordingSession() {
    if (!isRecording_) return;
    isRecording_ = false;

    if (micCapture_) micCapture_->Stop();
    if (loopbackCapture_) loopbackCapture_->StopCapture();

    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    encoder_.FinalizeAndClose();

    auto now = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(now - recordingStartTime_).count();
    if (currentCallDbId_ > 0) {
        dbLogger_.LogCallEnd(currentCallDbId_, duration);
    }

    ShowToastNotification(L"WhatsApp Call Recording Saved", L"Recording finalized and saved successfully.");
}

void TrayApp::AudioProcessingLoop() {
    // Process audio in 100ms frame batches at 48kHz (4800 frames)
    // Local Mic is 1-channel mono (4800 samples = 4800 frames)
    // Remote Loopback is 2-channel stereo (9600 samples = 4800 frames)
    constexpr size_t targetFrames = 4800;
    std::vector<int16_t> micBuf(targetFrames * 1);
    std::vector<int16_t> loopbackBuf(targetFrames * 2);

    while (isRecording_ || micRingBuffer_.AvailableRead() > 0 || loopbackRingBuffer_.AvailableRead() > 0) {
        size_t availableMicSamples = micRingBuffer_.AvailableRead();
        size_t availableLoopbackSamples = loopbackRingBuffer_.AvailableRead();

        size_t micFramesAvail = availableMicSamples;      // mono: 1 sample = 1 frame
        size_t loopbackFramesAvail = availableLoopbackSamples / 2; // stereo: 2 samples = 1 frame

        size_t framesToRead = std::min({targetFrames, micFramesAvail, loopbackFramesAvail});

        if (framesToRead > 0) {
            size_t micReadSamples = micRingBuffer_.Read(micBuf.data(), framesToRead * 1);
            size_t loopbackReadSamples = loopbackRingBuffer_.Read(loopbackBuf.data(), framesToRead * 2);

            auto stereoMux = AudioMultiplexer::Multiplex(
                micBuf.data(), micReadSamples, 1,
                loopbackBuf.data(), loopbackReadSamples, 2
            );

            encoder_.WritePCMFrame(stereoMux.data(), stereoMux.size());
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

int TrayApp::Run() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

} // namespace recorder
#endif // _WIN32
