#pragma once

// Shared state for the widget window. Split out of app.cpp only because that
// file grew past the size this project keeps its files under; the state itself
// is deliberately one place.

#include "layered_surface.h"
#include "render.h"
#include "settings.h"
#include "text_field.h"
#include "theme.h"
#include "tray_icon.h"

#include <windows.h>

#include <cstddef>
#include <string>

namespace offwork {

inline constexpr wchar_t kWindowClass[] = L"OffWork.Native.Widget";
inline constexpr wchar_t kWindowTitle[] = L"OffWork";
inline constexpr UINT_PTR kClockTimer = 1;
inline constexpr UINT_PTR kCaretTimer = 2;
inline constexpr UINT kCaretBlinkMs = 530;
inline constexpr std::size_t kTimeMaxLength = 5;
inline constexpr std::size_t kSalaryMaxLength = 16;
inline constexpr int kFieldCount = 3;

inline constexpr wchar_t kDefaultFooter[] = L"设置和窗口大小保存在本机";
inline constexpr wchar_t kValidationFooter[] = L"请输入有效时间、工作时长和日薪";

struct AppState {
    HWND window = nullptr;
    LayeredSurface surface;
    FontSet fonts;
    int fontScale = 0;
    HICON icon = nullptr;
    UINT dpi = 96;
    UINT taskbarCreatedMessage = 0;
    TrayIcon trayIcon;

    Settings settings;
    std::wstring settingsPath;

    bool expanded = false;
    bool pinned = true;
    bool autostartEnabled = false;
    EndInputMode editEndInputMode = EndInputMode::EndTime;
    HotElement hot = HotElement::None;

    FieldState fields[kFieldCount];
    int focusedField = -1;
    bool caretVisible = false;

    std::wstring footerText = kDefaultFooter;
    bool footerIsError = false;

    // Opacity slider drag.
    bool adjustingOpacity = false;

    // Text selection drag.
    bool selectingText = false;

    // Our own resize loop. DefWindowProc must never run its sizing loop: it
    // changes geometry first and leaves content for later, which is exactly the
    // stale frame this app had.
    bool resizing = false;
    WPARAM resizeHit = 0;
    POINT resizeAnchor = {0, 0};
};

// The single widget instance. This app is single-instance by design (see the
// mutex in main.cpp), so one global is the honest representation.
extern AppState g;

}  // namespace offwork
