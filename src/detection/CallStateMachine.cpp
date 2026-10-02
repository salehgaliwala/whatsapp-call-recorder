#include "detection/CallStateMachine.h"

namespace recorder {

CallStateMachine::CallStateMachine(double debounceTimeSeconds)
    : debounceTimeSeconds_(debounceTimeSeconds) {}

void CallStateMachine::SetStateChangedCallback(StateChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

CallState CallStateMachine::GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::string CallStateMachine::GetContactName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return contactName_;
}

void CallStateMachine::OnAudioSessionActive(bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    isAudioActive_ = active;
}

void CallStateMachine::OnCallUIPresent(bool present, const std::string& contactName) {
    std::lock_guard<std::mutex> lock(mutex_);
    isUIPresent_ = present;
    if (!contactName.empty()) {
        contactName_ = contactName;
    }
}

void CallStateMachine::TransitionTo(CallState newState) {
    if (state_ == newState) return;
    CallState oldState = state_;
    state_ = newState;

    if (callback_) {
        StateChangedCallback cb = callback_;
        mutex_.unlock();
        cb(oldState, newState);
        mutex_.lock();
    }
}

void CallStateMachine::Tick() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    switch (state_) {
        case CallState::Idle:
            if (isAudioActive_ && isUIPresent_) {
                detectionStartTime_ = now;
                TransitionTo(CallState::CallDetected);
            }
            break;

        case CallState::CallDetected:
            if (!isAudioActive_ || !isUIPresent_) {
                TransitionTo(CallState::Idle);
            } else {
                double elapsed = std::chrono::duration<double>(now - detectionStartTime_).count();
                if (elapsed >= debounceTimeSeconds_) {
                    TransitionTo(CallState::Recording);
                }
            }
            break;

        case CallState::Recording:
            if (!isAudioActive_ || !isUIPresent_) {
                TransitionTo(CallState::CallEnded);
            }
            break;

        case CallState::CallEnded:
            TransitionTo(CallState::FlushingEncoder);
            break;

        case CallState::FlushingEncoder:
            TransitionTo(CallState::Idle);
            break;
    }
}

void CallStateMachine::ForceEndCall() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == CallState::Recording || state_ == CallState::CallDetected) {
        TransitionTo(CallState::CallEnded);
        TransitionTo(CallState::FlushingEncoder);
        TransitionTo(CallState::Idle);
    }
}

} // namespace recorder
