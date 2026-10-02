# WhatsApp Auto Call Recorder (Windows Desktop)

A lightweight, production-grade C++20 Windows system tray desktop application that automatically detects and records voice/video calls from the WhatsApp PC desktop application (`WhatsApp.exe`).

---

## Technical Specifications & Architecture

- **Language Standard:** Modern C++20
- **Target OS:** Windows 10 (Build 20348+) / Windows 11 (x64)
- **Compiler / Toolchain:** Visual Studio 2022 (MSVC) / CMake (v3.20+)
- **Resource Footprint:** Idle RAM < 20 MB, near 0% CPU footprint while monitoring.
- **COM & Memory Management:** Strict RAII using WIL (`microsoft/wil`) and `Microsoft::WRL::ComPtr`. No raw COM pointers or manual reference counting.
- **Concurrency:** C++20 standard threading (`std::jthread`, `std::atomic`, `std::mutex`), lock-free circular buffer (`AudioRingBuffer`) for audio piping.

### Key Modules

1. **System Tray UI Shell (`src/ui/TrayApp`, `src/ui/SettingsDialog`)**:
   - Windowless background application using standard Win32 message loop.
   - System tray icon registration via `Shell_NotifyIconW` (`NIM_ADD`, `NIM_MODIFY`, `NIM_DELETE`).
   - Context menu: "Open Recordings Folder", "Settings / Device Select", "Status Indicator", "Exit".
   - Win32 toast/balloon notifications on call recording start and stop.
   - Settings modal dialog for configuring output directories and microphone device options.

2. **Call Detection Engine (`src/detection/CallStateMachine`, `src/detection/AudioSessionMonitor`, `src/detection/UIAutomationHook`)**:
   - **WASAPI Audio Session Monitor:** Uses `IAudioSessionManager2` and `IAudioSessionNotification` / `IAudioSessionEvents` to watch for active sessions from `WhatsApp.exe`.
   - **UI Automation Hook:** Queries Windows UI Automation (`IUIAutomation`) for WhatsApp windows, verifying call UI elements (buttons/automation IDs like "Mute", "End call", "Camera", "Video call") and extracting contact/group names.
   - **State Machine:** State transitions `IDLE` -> `CALL_DETECTED` -> `RECORDING` -> `CALL_ENDED` -> `FLUSHING_ENCODER` -> `IDLE` with a 2.5-second debounce timer to prevent false positives from voice notes or notification sounds.

3. **Dual-Stream WASAPI Audio Capture Engine (`src/audio/ProcessLoopbackCapture`, `src/audio/WasapiCapture`, `src/audio/AudioResampler`, `src/audio/AudioMultiplexer`)**:
   - **Remote Loopback (WhatsApp):** Uses `ActivateAudioInterfaceAsync` with `AUDIOCLIENT_ACTIVATION_PARAMS` (`AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`) to record WhatsApp audio without capturing system sounds.
   - **Local Mic:** Captures default communication microphone via WASAPI.
   - **Resampling & Multiplexing:** Resamples input streams to standard 48,000 Hz 16-bit PCM and multiplexes into a 2-channel stream:
     - **Channel 0 (Left):** Local microphone (outgoing).
     - **Channel 1 (Right):** Remote WhatsApp participant (incoming).

4. **Audio Encoding & Persistence (`src/encoder/MediaFoundationEncoder`, `src/db/SqliteLogger`)**:
   - **Encoder:** Uses Windows Media Foundation (`IMFSinkWriter`) to stream PCM frames directly to `.m4a` (AAC, 160 kbps, 48 kHz).
   - **File Naming:** `WhatsApp_Call_[ContactName]_[YYYY-MM-DD_HH-mm-ss].m4a`.
   - **Metadata Logging:** SQLite database (`recordings.db`) storing `call_id`, `contact_or_number`, `start_time`, `end_time`, `duration_seconds`, and `file_path`.

5. **Fault Tolerance (`src/audio/DeviceChangeMonitor`)**:
   - Handles audio device disconnects mid-call via `IMMNotificationClient`.
   - Graceful shutdown and buffer flushing on application exit or WhatsApp termination.

---

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── .gitignore
├── third_party/
│   ├── sqlite3.c
│   ├── sqlite3.h
│   └── sqlite3ext.h
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── Types.h
│   │   ├── Config.h / .cpp
│   │   └── AudioRingBuffer.h / .cpp
│   ├── audio/
│   │   ├── AudioResampler.h / .cpp
│   │   ├── AudioMultiplexer.h / .cpp
│   │   ├── WasapiCapture.h / .cpp
│   │   ├── ProcessLoopbackCapture.h / .cpp
│   │   └── DeviceChangeMonitor.h
│   ├── detection/
│   │   ├── CallStateMachine.h / .cpp
│   │   ├── AudioSessionMonitor.h / .cpp
│   │   └── UIAutomationHook.h / .cpp
│   ├── encoder/
│   │   └── MediaFoundationEncoder.h / .cpp
│   ├── db/
│   │   └── SqliteLogger.h / .cpp
│   └── ui/
│       ├── TrayApp.h / .cpp
│       └── SettingsDialog.h / .cpp
└── tests/
    ├── CMakeLists.txt
    └── test_core.cpp
```

---

## Build Instructions

### Requirements
- **Windows 10 / 11 (x64)**
- **Visual Studio 2022** with "Desktop development with C++" workload
- **CMake 3.20+**

### Building with Visual Studio / CMake

1. Open **Developer Command Prompt for VS 2022**.
2. Clone or navigate to the repository directory.
3. Generate the Visual Studio solution and build:

```cmd
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

4. Run unit tests:
```cmd
ctest --configuration Release --output-on-failure
```

5. The built executable `WhatsAppCallRecorder.exe` will be located in `build/Release/WhatsAppCallRecorder.exe`.
