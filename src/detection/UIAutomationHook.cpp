#if defined(_WIN32)
#include "detection/UIAutomationHook.h"
#include <wil/result.h>
#include <vector>
#include <algorithm>

namespace recorder {

static std::string Utf16ToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, nullptr, nullptr);
    return strTo;
}

UIAutomationHook::UIAutomationHook() = default;

UIAutomationHook::~UIAutomationHook() {
    Uninitialize();
}

HRESULT UIAutomationHook::Initialize() {
    if (automation_) return S_OK;
    return CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation_));
}

void UIAutomationHook::Uninitialize() {
    automation_ = nullptr;
}

bool UIAutomationHook::IsCallUIPresent(DWORD processId, std::string& outContactName) {
    outContactName = "Unknown_Contact";
    if (!automation_ || processId == 0) return false;

    Microsoft::WRL::ComPtr<IUIAutomationElement> root;
    if (FAILED(automation_->GetRootElement(&root)) || !root) return false;

    // Condition: ProcessId == WhatsApp PID
    VARIANT varPid;
    varPid.vt = VT_I4;
    varPid.lVal = static_cast<LONG>(processId);

    Microsoft::WRL::ComPtr<IUIAutomationCondition> pidCondition;
    if (FAILED(automation_->CreatePropertyCondition(UIA_ProcessIdPropertyId, varPid, &pidCondition))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IUIAutomationElementArray> windows;
    if (FAILED(root->FindAll(TreeScope_Children, pidCondition.Get(), &windows)) || !windows) {
        return false;
    }

    int count = 0;
    windows->get_Length(&count);
    if (count == 0) return false;

    bool foundCallUI = false;

    for (int i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IUIAutomationElement> window;
        if (SUCCEEDED(windows->GetElement(i, &window)) && window) {
            BSTR bstrName = nullptr;
            window->get_CurrentName(&bstrName);
            std::wstring windowName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);

            // Check if call UI controls exist in this window
            if (CheckCallButtonsOrTitles(window.Get())) {
                foundCallUI = true;
                std::string contact = ExtractContactName(window.Get());
                if (!contact.empty() && contact != "Unknown_Contact") {
                    outContactName = contact;
                } else if (!windowName.empty() && windowName != L"WhatsApp") {
                    outContactName = Utf16ToUtf8(windowName);
                }
                break;
            }
        }
    }

    return foundCallUI;
}

bool UIAutomationHook::CheckCallButtonsOrTitles(IUIAutomationElement* windowElement) {
    if (!windowElement || !automation_) return false;

    // Look for controls with AutomationID or Name matching "End call", "Mute", "Camera", "Video call", "Call info"
    const std::vector<std::wstring> callKeywords = {
        L"End call", L"Mute", L"Camera", L"Video call", L"Voice call", L"Call status", L"In call"
    };

    Microsoft::WRL::ComPtr<IUIAutomationCondition> trueCondition;
    automation_->CreateTrueCondition(&trueCondition);

    Microsoft::WRL::ComPtr<IUIAutomationElementArray> elements;
    if (FAILED(windowElement->FindAll(TreeScope_Subtree, trueCondition.Get(), &elements)) || !elements) {
        return false;
    }

    int count = 0;
    elements->get_Length(&count);

    for (int i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IUIAutomationElement> elem;
        if (SUCCEEDED(elements->GetElement(i, &elem)) && elem) {
            BSTR bstrName = nullptr;
            elem->get_CurrentName(&bstrName);
            std::wstring name = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);

            BSTR bstrAutoId = nullptr;
            elem->get_CurrentAutomationId(&bstrAutoId);
            std::wstring autoId = bstrAutoId ? bstrAutoId : L"";
            if (bstrAutoId) SysFreeString(bstrAutoId);

            for (const auto& kw : callKeywords) {
                if ((!name.empty() && name.find(kw) != std::wstring::npos) ||
                    (!autoId.empty() && autoId.find(kw) != std::wstring::npos)) {
                    return true;
                }
            }
        }
    }

    return false;
}

std::string UIAutomationHook::ExtractContactName(IUIAutomationElement* windowElement) {
    if (!windowElement || !automation_) return "Unknown_Contact";

    BSTR bstrName = nullptr;
    windowElement->get_CurrentName(&bstrName);
    std::wstring windowName = bstrName ? bstrName : L"";
    if (bstrName) SysFreeString(bstrName);

    // Sanitize contact name for file path
    std::string contact = Utf16ToUtf8(windowName);
    if (contact.empty() || contact == "WhatsApp") {
        contact = "Unknown_Contact";
    }

    // Sanitize filename characters: \ / : * ? " < > |
    std::string sanitized;
    for (char c : contact) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            sanitized += '_';
        } else {
            sanitized += c;
        }
    }

    return sanitized.empty() ? "Unknown_Contact" : sanitized;
}

} // namespace recorder
#endif // _WIN32
