#if defined(_WIN32)
#include "ui/TrayApp.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    recorder::TrayApp app;
    if (app.Initialize(hInstance)) {
        app.Run();
    }

    CoUninitialize();
    return 0;
}
#else
#include <iostream>

int main() {
    std::cout << "WhatsApp Auto Call Recorder Core Library compiled successfully on Linux platform." << std::endl;
    return 0;
}
#endif
