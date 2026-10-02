cmake_minimum_required(VERSION 3.20)
project(WhatsAppAutoCallRecorder VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Options
option(BUILD_TESTS "Build unit tests" ON)

# Third party SQLite3
add_library(sqlite3 STATIC third_party/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(sqlite3 PRIVATE ${CMAKE_DL_LIBS} pthread)
endif()

# Core include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Core sources (Cross-platform)
set(CORE_SOURCES
    src/core/Config.cpp
    src/core/AudioRingBuffer.cpp
    src/db/SqliteLogger.cpp
    src/audio/AudioResampler.cpp
    src/audio/AudioMultiplexer.cpp
    src/detection/CallStateMachine.cpp
)

add_library(recorder_core STATIC ${CORE_SOURCES})
target_link_libraries(recorder_core PUBLIC sqlite3)

if(WIN32)
    # Fetch WIL for Windows builds
    FetchContent_Declare(
        wil
        GIT_REPOSITORY https://github.com/microsoft/wil.git
        GIT_TAG        v1.0.240426.1
    )
    FetchContent_MakeAvailable(wil)

    set(WINDOWS_SOURCES
        src/audio/WasapiCapture.cpp
        src/audio/ProcessLoopbackCapture.cpp
        src/detection/AudioSessionMonitor.cpp
        src/detection/UIAutomationHook.cpp
        src/encoder/MediaFoundationEncoder.cpp
        src/ui/TrayApp.cpp
        src/ui/SettingsDialog.cpp
    )

    add_executable(WhatsAppCallRecorder WIN32
        src/main.cpp
        ${WINDOWS_SOURCES}
    )

    target_link_libraries(WhatsAppCallRecorder PRIVATE
        recorder_core
        WIL::WIL
        Ole32
        OleAut32
        Shell32
        User32
        Advapi32
        Mf
        Mfplat
        Mfreadwrite
        Mfuuid
        Propsys
        Mmdevapi
    )

    # Enable UTF-8 encoding for MSVC
    if(MSVC)
        target_compile_options(WhatsAppCallRecorder PRIVATE /utf-8 /W4)
        target_compile_options(recorder_core PRIVATE /utf-8 /W4)
    endif()
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
