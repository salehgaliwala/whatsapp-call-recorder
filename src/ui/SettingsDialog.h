#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#if defined(_WIN32)
#include <windows.h>
#include <string>

namespace recorder {

class SettingsDialog {
public:
    static bool Show(HWND parentWindow);
};

} // namespace recorder

#endif // _WIN32
#endif // SETTINGS_DIALOG_H
