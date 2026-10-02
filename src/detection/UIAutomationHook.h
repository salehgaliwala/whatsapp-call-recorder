#ifndef UI_AUTOMATION_HOOK_H
#define UI_AUTOMATION_HOOK_H

#if defined(_WIN32)
#include <windows.h>
#include <uiautomation.h>
#include <wrl/client.h>
#include <string>

namespace recorder {

class UIAutomationHook {
public:
    UIAutomationHook();
    ~UIAutomationHook();

    HRESULT Initialize();
    void Uninitialize();

    // Queries UI automation for process ID and inspects call window indicators.
    // Fills contactName with extracted contact/group name if found.
    bool IsCallUIPresent(DWORD processId, std::string& outContactName);

private:
    std::string ExtractContactName(IUIAutomationElement* windowElement);
    bool CheckCallButtonsOrTitles(IUIAutomationElement* windowElement);

    Microsoft::WRL::ComPtr<IUIAutomation> automation_;
};

} // namespace recorder

#endif // _WIN32
#endif // UI_AUTOMATION_HOOK_H
