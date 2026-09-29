#include "app_tray.h"

#include "app_edit.h"
#include "app_state.h"
#include "app_view.h"

#include <shellapi.h>
#include <windowsx.h>

namespace offwork {
namespace {

void ShowWidget() {
    if (g.window == nullptr) {
        return;
    }
    ShowWindow(g.window, SW_SHOWNORMAL);
    Present();
    EnsureTopmost();
    SetForegroundWindow(g.window);
}

void HandleTrayCommand(TrayCommand command) {
    switch (command) {
        case TrayCommand::ToggleWindow:
            if (IsWindowVisible(g.window)) {
                HideWidgetToTray();
            } else {
                ShowWidget();
            }
            break;
        case TrayCommand::ToggleAutostart:
            ToggleAutostart();
            if (g.footerIsError) {
                ShowWidget();
            }
            break;
        case TrayCommand::Exit:
            DestroyWindow(g.window);
            break;
        case TrayCommand::None:
            break;
    }
}

}  // namespace

void InitializeTray() {
    g.taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    g.trayIcon.Add(g.window, g.icon);
}

bool HandleTrayMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (g.taskbarCreatedMessage != 0 && message == g.taskbarCreatedMessage) {
        g.trayIcon.Reinstall();
        return true;
    }
    if (message != kTrayCallbackMessage) {
        return false;
    }

    const UINT event = LOWORD(lParam);
    if (event == WM_LBUTTONDBLCLK || event == NIN_KEYSELECT) {
        ShowWidget();
        return true;
    }
    if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) {
        RefreshAutostart();
        POINT anchor = {-1, -1};
        if (event == WM_CONTEXTMENU) {
            anchor = {GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
        }
        HandleTrayCommand(g.trayIcon.ShowMenu(
            IsWindowVisible(g.window) != FALSE,
            g.autostartEnabled, anchor));
        return true;
    }
    return true;
}

bool HideWidgetToTray() {
    if (g.window == nullptr || !g.trayIcon.IsAdded()) {
        return false;
    }
    SetFocusedField(-1);
    ShowWindow(g.window, SW_HIDE);
    return true;
}

void RemoveTray() {
    g.trayIcon.Remove();
}

}  // namespace offwork
