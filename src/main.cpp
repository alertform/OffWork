#include "app.h"

#include <windows.h>

namespace {

constexpr wchar_t kMutexName[] = L"Local\\OffWork.Native.SingleInstance";
constexpr wchar_t kWindowClass[] = L"OffWork.Native.Widget";
constexpr wchar_t kWindowTitle[] = L"OffWork";

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(kWindowClass, kWindowTitle);
            existing != nullptr) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(mutex);
        return 0;
    }

    const int result = offwork::RunApp(instance, showCommand);

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return result;
}
