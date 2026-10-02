#include "core/AudioRingBuffer.h"
#include "audio/AudioResampler.h"
#include "audio/AudioMultiplexer.h"
#include "db/SqliteLogger.h"
#include "detection/CallStateMachine.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <cmath>

void TestRingBuffer() {
    std::cout << "Testing AudioRingBuffer..." << std::endl;
    recorder::AudioRingBuffer ringBuffer(100);
    assert(ringBuffer.AvailableWrite() == 100);
    assert(ringBuffer.AvailableRead() == 0);

    int16_t inputData[50];
    for (int i = 0; i < 50; ++i) inputData[i] = static_cast<int16_t>(i);

    size_t written = ringBuffer.Write(inputData, 50);
    assert(written == 50);
    assert(ringBuffer.AvailableRead() == 50);

    int16_t outputData[50] = {0};
    size_t read = ringBuffer.Read(outputData, 50);
    assert(read == 50);
    assert(outputData[0] == 0 && outputData[49] == 49);
    assert(ringBuffer.AvailableRead() == 0);
    std::cout << "AudioRingBuffer passed!" << std::endl;
}

void TestAudioResampler() {
    std::cout << "Testing AudioResampler (44.1kHz -> 48kHz)..." << std::endl;
    recorder::AudioResampler resampler(44100, 48000, 1);
    std::vector<int16_t> monoIn(4410, 1000); // 0.1 sec at 44.1kHz
    auto resampled = resampler.Resample(monoIn.data(), monoIn.size());
    assert(!resampled.empty());
    // Expect around 4800 samples for 0.1 sec at 48kHz
    assert(resampled.size() >= 4790 && resampled.size() <= 4810);

    std::cout << "Testing AudioResampler Passthrough (48kHz -> 48kHz)..." << std::endl;
    recorder::AudioResampler resamplerPass(48000, 48000, 2);
    std::vector<int16_t> stereoIn = {100, 200, 300, 400};
    auto passOut = resamplerPass.Resample(stereoIn.data(), stereoIn.size());
    assert(passOut == stereoIn);

    std::cout << "AudioResampler passed!" << std::endl;
}

void TestAudioMultiplexer() {
    std::cout << "Testing AudioMultiplexer..." << std::endl;
    std::vector<int16_t> micMono = {100, 200, 300};
    std::vector<int16_t> remoteStereo = {10, 20, 30, 40, 50, 60}; // 3 stereo frames: (10,20), (30,40), (50,60)

    auto multiplexed = recorder::AudioMultiplexer::Multiplex(
        micMono.data(), micMono.size(), 1,
        remoteStereo.data(), remoteStereo.size(), 2
    );

    assert(multiplexed.size() == 6); // 3 stereo frames = 6 samples
    // Frame 0: Left = 100, Right = (10+20)/2 = 15
    assert(multiplexed[0] == 100 && multiplexed[1] == 15);
    // Frame 1: Left = 200, Right = (30+40)/2 = 35
    assert(multiplexed[2] == 200 && multiplexed[3] == 35);
    // Frame 2: Left = 300, Right = (50+60)/2 = 55
    assert(multiplexed[4] == 300 && multiplexed[5] == 55);

    std::cout << "AudioMultiplexer passed!" << std::endl;
}

void TestSqliteLogger() {
    std::cout << "Testing SqliteLogger..." << std::endl;
    recorder::SqliteLogger logger;
    assert(logger.Initialize(":memory:"));

    int64_t callId1 = logger.LogCallStart("Alice (+1234567890)", "/recordings/call_alice.m4a");
    assert(callId1 > 0);

    int64_t callId2 = logger.LogCallStart("Project Group Call", "/recordings/call_group.m4a");
    assert(callId2 > callId1);

    bool endOk1 = logger.LogCallEnd(callId1, 120.5);
    assert(endOk1);

    bool endOk2 = logger.LogCallEnd(callId2, 310.2);
    assert(endOk2);

    auto history = logger.GetCallHistory();
    assert(history.size() == 2);
    assert(history[0].contactOrNumber == "Project Group Call");
    assert(history[0].durationSeconds == 310.2);
    assert(history[1].contactOrNumber == "Alice (+1234567890)");
    assert(history[1].durationSeconds == 120.5);

    logger.Close();
    std::cout << "SqliteLogger passed!" << std::endl;
}

void TestCallStateMachine() {
    std::cout << "Testing CallStateMachine..." << std::endl;
    recorder::CallStateMachine sm(0.1); // 0.1 sec debounce for test
    recorder::CallState lastState = recorder::CallState::Idle;

    sm.SetStateChangedCallback([&](recorder::CallState oldState, recorder::CallState newState) {
        lastState = newState;
    });

    sm.OnAudioSessionActive(true);
    sm.OnCallUIPresent(true, "Bob");
    sm.Tick();
    assert(sm.GetState() == recorder::CallState::CallDetected);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    sm.Tick();
    assert(sm.GetState() == recorder::CallState::Recording);

    sm.OnAudioSessionActive(false);
    sm.Tick();
    assert(sm.GetState() == recorder::CallState::CallEnded);

    sm.Tick(); // CallEnded -> FlushingEncoder
    sm.Tick(); // FlushingEncoder -> Idle
    assert(sm.GetState() == recorder::CallState::Idle);

    std::cout << "CallStateMachine passed!" << std::endl;
}

int main() {
    TestRingBuffer();
    TestAudioResampler();
    TestAudioMultiplexer();
    TestSqliteLogger();
    TestCallStateMachine();
    std::cout << "\n===============================" << std::endl;
    std::cout << " ALL UNIT TESTS PASSED SUCCESSFULLY! " << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}
