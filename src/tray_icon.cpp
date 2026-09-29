#include "tray_icon.h"

#include <shellapi.h>

namespace offwork {
namespace {

constexpr UINT kTrayIconId = 1;
constexpr UINT kToggleWindowCommand = 1001;
constexpr UINT kToggleAutostartCommand = 1002;
constexpr UINT kExitCommand = 1003;

}  // namespace

bool TrayIcon::Add(HWND owner, HICON icon) {
    owner_ = owner;
    icon_ = icon;
    return AddToShell();
}

bool TrayIcon::AddToShell() {
    if (owner_ == nullptr || icon_ == nullptr) {
        added_ = false;
        return false;
    }

    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner_;
    data.uID = kTrayIconId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = kTrayCallbackMessage;
    data.hIcon = icon_;
    wcscpy_s(data.szTip, L"OffWork - 下班倒计时");

    added_ = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (added_) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
    return added_;
}

bool TrayIcon::ShowBalloon(const wchar_t* title, const wchar_t* body) {
    if (owner_ == nullptr || !added_) {
        return false;
    }
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner_;
    data.uID = kTrayIconId;
    data.uFlags = NIF_INFO;
    wcscpy_s(data.szInfoTitle, title);
    wcscpy_s(data.szInfo, body);
    // The cat, not the generic "i": NIIF_USER takes the balloon icon from
    // hBalloonIcon when NIIF_LARGE_ICON is set.
    data.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    data.hBalloonIcon = icon_;
    return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
}

void TrayIcon::Reinstall() {
    // Explorer recreates the notification area after a crash or restart and
    // broadcasts TaskbarCreated. Our bookkeeping still says "added", but the
    // shell no longer owns the icon, so issue NIM_ADD again.
    AddToShell();
}

void TrayIcon::Remove() {
    if (owner_ != nullptr && added_) {
        NOTIFYICONDATAW data = {};
        data.cbSize = sizeof(data);
        data.hWnd = owner_;
        data.uID = kTrayIconId;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    added_ = false;
}

TrayCommand TrayIcon::ShowMenu(
    bool windowVisible, bool autostartEnabled, POINT anchor) {
    if (owner_ == nullptr) {
        return TrayCommand::None;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return TrayCommand::None;
    }

    AppendMenuW(
        menu, MF_STRING | MF_DEFAULT, kToggleWindowCommand,
        windowVisible ? L"隐藏 OffWork" : L"显示 OffWork");
    AppendMenuW(
        menu, MF_STRING | (autostartEnabled ? MF_CHECKED : MF_UNCHECKED),
        kToggleAutostartCommand, L"开机自启");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, L"退出");

    if (anchor.x == -1 && anchor.y == -1) {
        GetCursorPos(&anchor);
    }

    // Required by TrackPopupMenu for a tray menu to dismiss correctly when
    // the user clicks elsewhere.
    SetForegroundWindow(owner_);
    const UINT selected = TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        anchor.x, anchor.y, owner_, nullptr);
    DestroyMenu(menu);
    PostMessageW(owner_, WM_NULL, 0, 0);

    switch (selected) {
        case kToggleWindowCommand:
            return TrayCommand::ToggleWindow;
        case kToggleAutostartCommand:
            return TrayCommand::ToggleAutostart;
        case kExitCommand:
            return TrayCommand::Exit;
        default:
            return TrayCommand::None;
    }
}

}  // namespace offwork
