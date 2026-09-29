#include "app_tray.h"

#include "app_edit.h"
#include "app_state.h"
#include "app_view.h"
#include "calculator.h"
#include "reminder.h"

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
    if (event == WM_LBUTTONDBLCLK || event == NIN_KEYSELECT ||
        event == NIN_BALLOONUSERCLICK) {
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

namespace {

struct PhaseNow {
    WorkdayPhase phase = WorkdayPhase::AfterWork;
    int secondsSinceEnd = 0;
};

PhaseNow CurrentPhase() {
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    const int nowSeconds = now.wHour * 3600 + now.wMinute * 60 + now.wSecond;
    PhaseNow result;
    try {
        result.phase = CalculateWorkday(
            nowSeconds, g.settings.startMinutes, g.settings.endMinutes,
            g.settings.dailySalary).phase;
    } catch (...) {
        // NormalizeSettings makes this unreachable; never let a clock tick
        // take the process down.
        result.phase = WorkdayPhase::AfterWork;
    }
    result.secondsSinceEnd = SecondsSinceShiftEnd(nowSeconds, g.settings.endMinutes);
    return result;
}

}  // namespace

void ResetShiftReminder() {
    g.reminder = ResetReminder(CurrentPhase().phase);
}

void TickShiftReminder() {
    const PhaseNow now = CurrentPhase();
    const ReminderDecision decision =
        TickReminder(g.reminder, now.phase, now.secondsSinceEnd);
    g.reminder = decision.next;
    if (decision.notify) {
        const bool accepted =
            g.trayIcon.ShowBalloon(L"可以下班啦", L"今天辛苦了，记得打卡下班。");
        // Visible only to a debugger or debug-output listener. Whether Windows
        // then *displays* the banner is its policy (Focus Assist), so this is
        // the one place that can say the app did its part.
        OutputDebugStringW(accepted
            ? L"OffWork: off-work reminder submitted (Shell_NotifyIcon NIF_INFO accepted)\n"
            : L"OffWork: off-work reminder NOT accepted by Shell_NotifyIcon\n");
    }
}

}  // namespace offwork
