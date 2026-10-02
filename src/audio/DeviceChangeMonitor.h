#ifndef DEVICE_CHANGE_MONITOR_H
#define DEVICE_CHANGE_MONITOR_H

#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <functional>

namespace recorder {

class DeviceChangeMonitor :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMMNotificationClient
    >
{
public:
    using DeviceChangedCallback = std::function<void(EDataFlow flow, ERole role, LPCWSTR deviceId)>;

    DeviceChangeMonitor() = default;
    ~DeviceChangeMonitor() { Stop(); }

    HRESULT Initialize(DeviceChangedCallback callback) {
        callback_ = std::move(callback);
        RETURN_IF_FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator_)));
        return enumerator_->RegisterEndpointNotificationCallback(this);
    }

    void Stop() {
        if (enumerator_) {
            enumerator_->UnregisterEndpointNotificationCallback(this);
            enumerator_ = nullptr;
        }
    }

    // IMMNotificationClient
    IFACEMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override {
        if (callback_) {
            callback_(flow, role, pwstrDefaultDeviceId);
        }
        return S_OK;
    }
    IFACEMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId) override { return S_OK; }
    IFACEMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId) override { return S_OK; }
    IFACEMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override { return S_OK; }
    IFACEMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override { return S_OK; }

private:
    DeviceChangedCallback callback_;
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
};

} // namespace recorder

#endif // _WIN32
#endif // DEVICE_CHANGE_MONITOR_H
