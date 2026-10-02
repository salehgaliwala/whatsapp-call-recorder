#ifndef CALL_STATE_MACHINE_H
#define CALL_STATE_MACHINE_H

#include "core/Types.h"
#include <functional>
#include <mutex>
#include <chrono>

namespace recorder {

class CallStateMachine {
public:
    using StateChangedCallback = std::function<void(CallState oldState, CallState newState)>;

    explicit CallStateMachine(double debounceTimeSeconds = 2.5);

    void SetStateChangedCallback(StateChangedCallback callback);

    CallState GetState() const;
    std::string GetContactName() const;

    // Signal inputs from audio session & UI automation
    void OnAudioSessionActive(bool active);
    void OnCallUIPresent(bool present, const std::string& contactName = "");

    // Tick method to process debounce timer transitions
    void Tick();

    void ForceEndCall();

private:
    void TransitionTo(CallState newState);

    mutable std::mutex mutex_;
    CallState state_{CallState::Idle};
    double debounceTimeSeconds_{2.5};

    bool isAudioActive_{false};
    bool isUIPresent_{false};
    std::string contactName_{"Unknown_Contact"};

    std::chrono::steady_clock::time_point detectionStartTime_;
    StateChangedCallback callback_;
};

} // namespace recorder

#endif // CALL_STATE_MACHINE_H
