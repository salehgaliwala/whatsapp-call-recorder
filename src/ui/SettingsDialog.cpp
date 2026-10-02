#if defined(_WIN32)
#include "ui/SettingsDialog.h"
#include "core/Config.h"
#include <shlobj.h>
#include <commctrl.h>

namespace recorder {

#define IDC_EDIT_DIR 1001
#define IDC_BTN_BROWSE 1002
#define IDC_COMBO_MIC 1003
#define IDC_BTN_OK 1004
#define IDC_BTN_CANCEL 1005

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            AppConfig cfg = Config::Instance().Get();
            SetDlgItemTextA(hDlg, IDC_EDIT_DIR, cfg.outputDirectory.c_str());

            HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_MIC);
            if (hCombo) {
                SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"Default Communication Microphone");
                SendMessageA(hCombo, CB_SETCURSEL, 0, 0);
            }

            RECT rcParent, rcDlg;
            GetWindowRect(GetDesktopWindow(), &rcParent);
            GetWindowRect(hDlg, &rcDlg);
            int x = (rcParent.right - (rcDlg.right - rcDlg.left)) / 2;
            int y = (rcParent.bottom - (rcDlg.bottom - rcDlg.top)) / 2;
            SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return TRUE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDC_BTN_BROWSE) {
                BROWSEINFOA bi = {0};
                bi.hwndOwner = hDlg;
                bi.lpszTitle = "Select Recordings Folder";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
                if (pidl) {
                    char path[MAX_PATH];
                    if (SHGetPathFromIDListA(pidl, path)) {
                        SetDlgItemTextA(hDlg, IDC_EDIT_DIR, path);
                    }
                    CoTaskMemFree(pidl);
                }
            } else if (id == IDC_BTN_OK) {
                char dirBuf[MAX_PATH] = {0};
                GetDlgItemTextA(hDlg, IDC_EDIT_DIR, dirBuf, MAX_PATH);

                AppConfig cfg = Config::Instance().Get();
                cfg.outputDirectory = dirBuf;
                Config::Instance().Update(cfg);

                EndDialog(hDlg, IDOK);
                return TRUE;
            } else if (id == IDC_BTN_CANCEL || id == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

#pragma pack(push, 4)
struct DLGTEMPLATEEX {
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cdit;
    short x;
    short y;
    short cx;
    short cy;
};

struct DLGITEMTEMPLATEEX {
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    short x;
    short y;
    short cx;
    short cy;
    DWORD id;
};
#pragma pack(pop)

bool SettingsDialog::Show(HWND parentWindow) {
    BYTE buffer[2048] = {0};
    BYTE* p = buffer;

    DLGTEMPLATEEX* pDlg = reinterpret_cast<DLGTEMPLATEEX*>(p);
    pDlg->dlgVer = 1;
    pDlg->signature = 0xFFFF;
    pDlg->helpID = 0;
    pDlg->exStyle = 0;
    pDlg->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER;
    pDlg->cdit = 5; // 5 controls
    pDlg->x = 0; pDlg->y = 0;
    pDlg->cx = 260; pDlg->cy = 130;
    p += sizeof(DLGTEMPLATEEX);

    // Menu array (0 bytes), Class array (0 bytes), Title (L"Settings / Device Select")
    *reinterpret_cast<WORD*>(p) = 0; p += 2;
    *reinterpret_cast<WORD*>(p) = 0; p += 2;
    const wchar_t* title = L"Settings / Device Select";
    wcscpy_s(reinterpret_cast<wchar_t*>(p), 32, title);
    p += (wcslen(title) + 1) * sizeof(wchar_t);

    // Font size & name
    *reinterpret_cast<WORD*>(p) = 9; p += 2; // point size
    *reinterpret_cast<WORD*>(p) = FW_NORMAL; p += 2; // weight
    *reinterpret_cast<BYTE*>(p) = 0; p += 1; // italic
    *reinterpret_cast<BYTE*>(p) = DEFAULT_CHARSET; p += 1; // charset
    const wchar_t* fontName = L"Segoe UI";
    wcscpy_s(reinterpret_cast<wchar_t*>(p), 16, fontName);
    p += (wcslen(fontName) + 1) * sizeof(wchar_t);

    auto AlignDword = [](BYTE*& ptr) {
        uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t aligned = (address + 3) & ~3;
        ptr = reinterpret_cast<BYTE*>(aligned);
    };

    auto AddControl = [&](DWORD style, DWORD exStyle, short x, short y, short cx, short cy, DWORD id, WORD classAtom, const wchar_t* caption) {
        AlignDword(p);
        DLGITEMTEMPLATEEX* pItem = reinterpret_cast<DLGITEMTEMPLATEEX*>(p);
        pItem->helpID = 0;
        pItem->exStyle = exStyle;
        pItem->style = style | WS_CHILD | WS_VISIBLE;
        pItem->x = x; pItem->y = y;
        pItem->cx = cx; pItem->cy = cy;
        pItem->id = id;
        p += sizeof(DLGITEMTEMPLATEEX);

        *reinterpret_cast<WORD*>(p) = 0xFFFF; p += 2;
        *reinterpret_cast<WORD*>(p) = classAtom; p += 2;

        if (caption) {
            wcscpy_s(reinterpret_cast<wchar_t*>(p), 64, caption);
            p += (wcslen(caption) + 1) * sizeof(wchar_t);
        } else {
            *reinterpret_cast<WORD*>(p) = 0; p += 2;
        }
        *reinterpret_cast<WORD*>(p) = 0; p += 2; // extra creation data
    };

    // Control 1: Edit box for dir
    AddControl(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 10, 15, 180, 14, IDC_EDIT_DIR, 0x0081, nullptr);
    // Control 2: Browse button
    AddControl(BS_PUSHBUTTON | WS_TABSTOP, 0, 195, 14, 55, 16, IDC_BTN_BROWSE, 0x0080, L"Browse...");
    // Control 3: ComboBox for mic
    AddControl(CBS_DROPDOWNLIST | WS_TABSTOP, 0, 10, 45, 240, 60, IDC_COMBO_MIC, 0x0085, nullptr);
    // Control 4: OK button
    AddControl(BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 130, 95, 55, 16, IDC_BTN_OK, 0x0080, L"OK");
    // Control 5: Cancel button
    AddControl(BS_PUSHBUTTON | WS_TABSTOP, 0, 195, 95, 55, 16, IDC_BTN_CANCEL, 0x0080, L"Cancel");

    INT_PTR res = DialogBoxIndirectParamW(
        GetModuleHandle(NULL),
        reinterpret_cast<DLGTEMPLATE*>(buffer),
        parentWindow,
        SettingsDlgProc,
        0
    );

    return (res == IDOK);
}

} // namespace recorder
#endif // _WIN32
