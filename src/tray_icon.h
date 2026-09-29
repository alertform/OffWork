#pragma once

// System-tray ownership for the unpackaged Win32 application. The icon stays
// present while OffWork is running, including while the widget is hidden.

#include <windows.h>

namespace offwork {

inline constexpr UINT kTrayCallbackMessage = WM_APP + 1;

enum class TrayCommand {
    None,
    ToggleWindow,
    ToggleAutostart,
    Exit,
};

class TrayIcon {
public:
    bool Add(HWND owner, HICON icon);
    void Reinstall();
    void Remove();

    [[nodiscard]] bool IsAdded() const { return added_; }

    TrayCommand ShowMenu(bool windowVisible, bool autostartEnabled, POINT anchor);

    // Submits a notification banner through the notification-area icon.
    // Returns whether the shell accepted it; whether it is *displayed* is up to
    // Windows (Focus Assist / notification settings), not to us.
    bool ShowBalloon(const wchar_t* title, const wchar_t* body);

private:
    bool AddToShell();

    HWND owner_ = nullptr;
    HICON icon_ = nullptr;
    bool added_ = false;
};

}  // namespace offwork
