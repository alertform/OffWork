#include "app.h"

#include "autostart.h"
#include "layered_surface.h"
#include "render.h"
#include "resource.h"
#include "settings.h"
#include "text_field.h"
#include "theme.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

namespace offwork {
namespace {

constexpr wchar_t kWindowClass[] = L"OffWork.Native.Widget";
constexpr wchar_t kWindowTitle[] = L"OffWork";
constexpr UINT_PTR kClockTimer = 1;
constexpr UINT_PTR kCaretTimer = 2;
constexpr UINT kCaretBlinkMs = 530;
constexpr std::size_t kTimeMaxLength = 5;
constexpr std::size_t kSalaryMaxLength = 16;
constexpr int kFieldCount = 3;

const wchar_t kDefaultFooter[] = L"设置和窗口大小保存在本机";
const wchar_t kValidationFooter[] = L"请输入 HH:MM 时间和有效日薪";

struct AppState {
    HWND window = nullptr;
    LayeredSurface surface;
    FontSet fonts;
    int fontScale = 0;
    HICON icon = nullptr;
    UINT dpi = 96;

    Settings settings;
    std::wstring settingsPath;

    bool expanded = false;
    bool pinned = true;
    bool autostartEnabled = false;
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

AppState g;

// -- small helpers ----------------------------------------------------------

FieldFilter FilterFor(int fieldIndex) {
    return fieldIndex == 2 ? FieldFilter::Decimal : FieldFilter::Time;
}

std::size_t MaxLengthFor(int fieldIndex) {
    return fieldIndex == 2 ? kSalaryMaxLength : kTimeMaxLength;
}

std::wstring FormatTimeValue(int minutes) {
    wchar_t text[16] = {};
    swprintf_s(text, L"%02d:%02d", minutes / 60, minutes % 60);
    return text;
}

std::wstring FormatSalaryValue(double salary) {
    wchar_t text[64] = {};
    swprintf_s(text, L"%.2f", salary);
    std::wstring value(text);
    while (value.size() > 1 && value.back() == L'0') {
        value.pop_back();
    }
    if (!value.empty() && value.back() == L'.') {
        value.pop_back();
    }
    return value;
}

void PopulateFields() {
    g.fields[0] = MakeField(FormatTimeValue(g.settings.startMinutes));
    g.fields[1] = MakeField(FormatTimeValue(g.settings.endMinutes));
    g.fields[2] = MakeField(FormatSalaryValue(g.settings.dailySalary));
}

bool ParseTimeText(const std::wstring& text, int& minutes) {
    int hour = -1;
    int minute = -1;
    wchar_t trailing = L'\0';
    if (swscanf_s(text.c_str(), L"%d:%d %c", &hour, &minute, &trailing, 1) != 2 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    minutes = hour * 60 + minute;
    return true;
}

bool ReadFieldSettings(Settings& settings) {
    settings = g.settings;
    if (!ParseTimeText(g.fields[0].text, settings.startMinutes) ||
        !ParseTimeText(g.fields[1].text, settings.endMinutes) ||
        settings.startMinutes == settings.endMinutes) {
        return false;
    }

    const std::wstring& salaryText = g.fields[2].text;
    if (salaryText.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const double salary = std::wcstod(salaryText.c_str(), &end);
    if (end == nullptr || *end != L'\0' || !std::isfinite(salary) ||
        salary < 0.0 || salary > 1000000.0) {
        return false;
    }
    settings.dailySalary = salary;
    return true;
}

void EnsureFonts(int scalePercent) {
    if (g.fontScale == scalePercent && g.fonts.label != nullptr) {
        return;
    }
    DestroyFonts(g.fonts);
    g.fonts = CreateFonts(scalePercent, g.dpi);
    g.fontScale = scalePercent;
}

RenderModel BuildModel() {
    RenderModel model;
    model.settings = g.settings;
    model.dpi = g.dpi;
    model.expanded = g.expanded;
    model.pinned = g.pinned;
    model.autostartEnabled = g.autostartEnabled;
    model.hot = g.hot;
    model.icon = g.icon;
    model.fonts = &g.fonts;
    model.startField = g.fields[0];
    model.endField = g.fields[1];
    model.salaryField = g.fields[2];
    model.focusedField = g.expanded ? g.focusedField : -1;
    model.caretVisible = g.caretVisible;
    model.footerText = g.footerText;
    model.footerIsError = g.footerIsError;
    GetLocalTime(&model.now);
    return model;
}

// The single place where anything reaches the screen. Size, position and
// pixels always leave together.
void PresentAt(POINT topLeft, int scalePercent) {
    if (g.window == nullptr) {
        return;
    }
    g.settings.uiScalePercent =
        std::clamp(scalePercent, kMinScalePercent, kMaxScalePercent);
    EnsureFonts(g.settings.uiScalePercent);

    const SIZE size = WindowSizeFor(g.settings.uiScalePercent, g.dpi, g.expanded);
    if (!g.surface.EnsureSize(size.cx, size.cy)) {
        return;
    }

    const RenderModel model = BuildModel();
    RenderFrame(g.surface.dc(), model);
    g.surface.ApplyRoundedAlpha(
        ScaleValue(kCornerRadius, g.settings.uiScalePercent, g.dpi));

    const auto alpha = static_cast<BYTE>(
        MulDiv(std::clamp(g.settings.opacityPercent, kMinOpacityPercent, 100), 255, 100));
    g.surface.Present(g.window, topLeft, alpha);
}

void Present() {
    RECT rect = {};
    if (g.window == nullptr || GetWindowRect(g.window, &rect) == 0) {
        return;
    }
    PresentAt({rect.left, rect.top}, g.settings.uiScalePercent);
}

void SetFooter(std::wstring text, bool isError) {
    g.footerText = std::move(text);
    g.footerIsError = isError;
}

void ResetFooter() {
    SetFooter(kDefaultFooter, false);
}

void RefreshAutostart() {
    g.autostartEnabled = IsAutostartEnabled();
}

void SetFocusedField(int index) {
    if (g.focusedField == index) {
        return;
    }
    g.focusedField = index;
    g.caretVisible = index >= 0;
    if (index >= 0) {
        SetTimer(g.window, kCaretTimer, kCaretBlinkMs, nullptr);
    } else {
        KillTimer(g.window, kCaretTimer);
        g.caretVisible = false;
    }
}

void SetExpanded(bool expanded) {
    if (g.expanded == expanded) {
        return;
    }
    g.expanded = expanded;
    if (expanded) {
        PopulateFields();
        RefreshAutostart();
        ResetFooter();
        SetFocusedField(-1);
    } else {
        SetFocusedField(-1);
        ResetFooter();
    }
    // Collapsing and expanding are just another atomic size change.
    Present();
}

void EnsureTopmost() {
    if (g.pinned && g.window != nullptr) {
        SetWindowPos(
            g.window, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// -- clipboard --------------------------------------------------------------

std::wstring ReadClipboardText(HWND owner) {
    if (OpenClipboard(owner) == 0) {
        return {};
    }
    std::wstring text;
    if (HANDLE handle = GetClipboardData(CF_UNICODETEXT); handle != nullptr) {
        if (const auto* data = static_cast<const wchar_t*>(GlobalLock(handle));
            data != nullptr) {
            text = data;
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    // A pasted newline would otherwise be silently dropped mid-string.
    const std::size_t breakAt = text.find_first_of(L"\r\n");
    if (breakAt != std::wstring::npos) {
        text.resize(breakAt);
    }
    return text;
}

void WriteClipboardText(HWND owner, const std::wstring& text) {
    if (text.empty() || OpenClipboard(owner) == 0) {
        return;
    }
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes); handle != nullptr) {
        if (auto* data = static_cast<wchar_t*>(GlobalLock(handle)); data != nullptr) {
            memcpy(data, text.c_str(), bytes);
            GlobalUnlock(handle);
            SetClipboardData(CF_UNICODETEXT, handle);
        } else {
            GlobalFree(handle);
        }
    }
    CloseClipboard();
}

// -- actions ----------------------------------------------------------------

void ApplySave() {
    Settings parsed;
    if (!ReadFieldSettings(parsed)) {
        SetFooter(kValidationFooter, true);
        Present();
        return;
    }
    g.settings = NormalizeSettings(parsed);
    SaveSettings(g.settingsPath, g.settings);
    SetExpanded(false);
}

void ToggleAutostart() {
    const bool target = !g.autostartEnabled;
    const AutostartResult result = SetAutostartEnabled(target);
    RefreshAutostart();
    if (!result.ok) {
        SetFooter(result.message, true);
    } else if (g.autostartEnabled != target) {
        SetFooter(L"开机自启未生效, 请检查系统设置", true);
    } else {
        ResetFooter();
    }
    Present();
}

void TogglePinned() {
    g.pinned = !g.pinned;
    SetWindowPos(
        g.window, g.pinned ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    Present();
}

// -- resize -----------------------------------------------------------------

bool IsResizeHit(WPARAM hit) {
    switch (hit) {
        case HTLEFT:
        case HTRIGHT:
        case HTTOP:
        case HTBOTTOM:
        case HTTOPLEFT:
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
        case HTBOTTOMRIGHT:
            return true;
        default:
            return false;
    }
}

bool ResizesFromLeft(WPARAM hit) {
    return hit == HTLEFT || hit == HTTOPLEFT || hit == HTBOTTOMLEFT;
}

bool ResizesFromTop(WPARAM hit) {
    return hit == HTTOP || hit == HTTOPLEFT || hit == HTTOPRIGHT;
}

bool UsesWidth(WPARAM hit) {
    return hit != HTTOP && hit != HTBOTTOM;
}

bool UsesHeight(WPARAM hit) {
    return hit != HTLEFT && hit != HTRIGHT;
}

void BeginResize(WPARAM hit) {
    RECT rect = {};
    if (GetWindowRect(g.window, &rect) == 0) {
        return;
    }
    g.resizing = true;
    g.resizeHit = hit;
    // The corner or edge that must not move while the opposite one follows the
    // cursor.
    g.resizeAnchor.x = ResizesFromLeft(hit) ? rect.right : rect.left;
    g.resizeAnchor.y = ResizesFromTop(hit) ? rect.bottom : rect.top;
    SetCapture(g.window);
}

// `cursor` is in screen coordinates and comes from the WM_MOUSEMOVE being
// handled, not from GetCursorPos. Using the message's own position is both the
// correct Win32 idiom and what makes the resize verifiable: a harness can drive
// the loop with messages addressed to this window instead of moving the real
// pointer across the user's desktop.
void UpdateResize(POINT cursor) {
    const SIZE base = WindowSizeFor(100, g.dpi, g.expanded);
    if (base.cx <= 0 || base.cy <= 0) {
        return;
    }

    const int widthFromCursor = std::abs(cursor.x - g.resizeAnchor.x);
    const int heightFromCursor = std::abs(cursor.y - g.resizeAnchor.y);
    const int widthPercent = MulDiv(widthFromCursor, 100, base.cx);
    const int heightPercent = MulDiv(heightFromCursor, 100, base.cy);

    int percent = g.settings.uiScalePercent;
    if (UsesWidth(g.resizeHit) && UsesHeight(g.resizeHit)) {
        // Project the cursor onto the proportional diagonal: the scale whose
        // window corner sits closest to where the pointer actually is.
        //
        // The obvious alternative -- "follow whichever axis moved further from
        // the current scale" -- is what this code used to do, inherited from
        // the old ConstrainSizingRect. It feeds its own output back in, so a
        // cursor off the diagonal makes it oscillate forever between two
        // scales: width says 101%, height says 81%, and each answer makes the
        // other one look further away on the next message. The system sizing
        // loop used to hide that by handing over a fresh proposed rectangle
        // every time; driving the resize ourselves exposed it.
        const long long dx = widthFromCursor;
        const long long dy = heightFromCursor;
        const long long numerator = dx * base.cx + dy * base.cy;
        const long long denominator =
            static_cast<long long>(base.cx) * base.cx +
            static_cast<long long>(base.cy) * base.cy;
        percent = static_cast<int>(
            (numerator * 100 + denominator / 2) / denominator);
    } else if (UsesWidth(g.resizeHit)) {
        percent = widthPercent;
    } else {
        percent = heightPercent;
    }
    percent = std::clamp(percent, kMinScalePercent, kMaxScalePercent);


    const SIZE size = WindowSizeFor(percent, g.dpi, g.expanded);
    POINT topLeft = {
        ResizesFromLeft(g.resizeHit) ? g.resizeAnchor.x - size.cx : g.resizeAnchor.x,
        ResizesFromTop(g.resizeHit) ? g.resizeAnchor.y - size.cy : g.resizeAnchor.y};

    // One call: the frame the compositor shows next already has this size.
    PresentAt(topLeft, percent);
}

void EndResize() {
    if (!g.resizing) {
        return;
    }
    g.resizing = false;
    if (GetCapture() == g.window) {
        ReleaseCapture();
    }
    SaveSettings(g.settingsPath, g.settings);
}

// -- keyboard ---------------------------------------------------------------

void FocusNextField(int direction) {
    if (!g.expanded) {
        return;
    }
    int index = g.focusedField;
    if (index < 0) {
        index = direction > 0 ? 0 : kFieldCount - 1;
    } else {
        index = (index + direction + kFieldCount) % kFieldCount;
    }
    SetFocusedField(index);
    g.fields[index] = SelectAll(g.fields[index]);
    Present();
}

bool HandleFieldKey(WPARAM key) {
    const int index = g.focusedField;
    if (index < 0 || index >= kFieldCount) {
        return false;
    }
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    FieldState& field = g.fields[index];

    switch (key) {
        case VK_LEFT:
            field = MoveCaret(field, -1, shift);
            break;
        case VK_RIGHT:
            field = MoveCaret(field, 1, shift);
            break;
        case VK_HOME:
            field = MoveToStart(field, shift);
            break;
        case VK_END:
            field = MoveToEnd(field, shift);
            break;
        case VK_BACK:
            field = DeleteBackward(field);
            break;
        case VK_DELETE:
            field = DeleteForward(field);
            break;
        case 'A':
            if (!control) {
                return false;
            }
            field = SelectAll(field);
            break;
        case 'C':
            if (!control) {
                return false;
            }
            WriteClipboardText(g.window, SelectedText(field));
            break;
        case 'X':
            if (!control) {
                return false;
            }
            WriteClipboardText(g.window, SelectedText(field));
            field = DeleteSelection(field);
            break;
        case 'V': {
            if (!control) {
                return false;
            }
            const std::wstring pasted = ReadClipboardText(g.window);
            field = InsertText(field, pasted, FilterFor(index), MaxLengthFor(index));
            break;
        }
        default:
            return false;
    }

    if (g.footerIsError) {
        ResetFooter();
    }
    g.caretVisible = true;
    SetTimer(g.window, kCaretTimer, kCaretBlinkMs, nullptr);
    Present();
    return true;
}

// -- window proc ------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g.window = window;
            g.dpi = GetDpiForWindow(window);
            g.icon = static_cast<HICON>(LoadImageW(
                GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_OFFWORK),
                IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR));
            PopulateFields();
            RefreshAutostart();
            EnsureFonts(g.settings.uiScalePercent);
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(window, 20, &darkMode, sizeof(darkMode));
            SetTimer(window, kClockTimer, 1000, nullptr);
            return 0;
        }

        case WM_TIMER:
            if (wParam == kClockTimer) {
                Present();
            } else if (wParam == kCaretTimer) {
                g.caretVisible = !g.caretVisible;
                Present();
            }
            return 0;

        case WM_PAINT: {
            // Content is published by UpdateLayeredWindow, never by WM_PAINT.
            PAINTSTRUCT paint = {};
            BeginPaint(window, &paint);
            EndPaint(window, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_NCCALCSIZE:
            // Belt and braces: the window has no WS_THICKFRAME any more, so
            // there is no non-client area to subtract in the first place.
            if (wParam != 0) {
                return 0;
            }
            break;

        case WM_SETCURSOR: {
            // DefWindowProc only supplies sizing cursors for a window that has
            // WS_THICKFRAME. This one does not, because that style cost 7px of
            // non-client border on every edge, so the cursors are ours to set.
            LPCWSTR cursor = nullptr;
            switch (LOWORD(lParam)) {
                case HTLEFT:
                case HTRIGHT:
                    cursor = IDC_SIZEWE;
                    break;
                case HTTOP:
                case HTBOTTOM:
                    cursor = IDC_SIZENS;
                    break;
                case HTTOPLEFT:
                case HTBOTTOMRIGHT:
                    cursor = IDC_SIZENWSE;
                    break;
                case HTTOPRIGHT:
                case HTBOTTOMLEFT:
                    cursor = IDC_SIZENESW;
                    break;
                default:
                    break;
            }
            if (cursor != nullptr) {
                SetCursor(LoadCursorW(nullptr, cursor));
                return TRUE;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(window, &point);
            RECT client = {};
            GetClientRect(window, &client);
            const int border =
                std::max(4, ScaleValue(6, g.settings.uiScalePercent, g.dpi));
            const bool left = point.x < border;
            const bool right = point.x >= client.right - border;
            const bool top = point.y < border;
            const bool bottom = point.y >= client.bottom - border;

            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;

            const RenderModel model = BuildModel();
            if (point.y < ScaleValue(56, g.settings.uiScalePercent, g.dpi) &&
                HitTest(point, model) == HotElement::None) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_NCLBUTTONDOWN:
            // Taking the drag away from DefWindowProc is the whole fix.
            if (IsResizeHit(wParam)) {
                BeginResize(wParam);
                return 0;
            }
            break;

        case WM_SYSCOMMAND:
            // SC_SIZE would hand the sizing loop back to the system.
            if ((wParam & 0xFFF0) == SC_SIZE) {
                return 0;
            }
            break;

        case WM_MOUSEMOVE: {
            POINT client = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            if (g.resizing) {
                POINT cursor = client;
                ClientToScreen(window, &cursor);
                UpdateResize(cursor);
                return 0;
            }

            if (g.adjustingOpacity) {
                const int opacity =
                    OpacityFromX(client.x, g.settings.uiScalePercent, g.dpi);
                if (opacity != g.settings.opacityPercent) {
                    g.settings.opacityPercent = opacity;
                    Present();
                }
                return 0;
            }

            if (g.selectingText && g.focusedField >= 0) {
                HDC dc = g.surface.dc();
                if (dc != nullptr) {
                    const RenderModel model = BuildModel();
                    const std::size_t caret =
                        CaretIndexFromX(dc, model, g.focusedField, client.x);
                    g.fields[g.focusedField] =
                        MoveCaretTo(g.fields[g.focusedField], caret, true);
                    Present();
                }
                return 0;
            }

            const RenderModel model = BuildModel();
            const HotElement hot = HitTest(client, model);
            if (hot != g.hot) {
                g.hot = hot;
                Present();
            }
            TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g.hot != HotElement::None) {
                g.hot = HotElement::None;
                Present();
            }
            return 0;

        case WM_LBUTTONDOWN: {
            POINT client = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const RenderModel model = BuildModel();
            const HotElement hot = HitTest(client, model);

            if (hot == HotElement::Opacity) {
                g.adjustingOpacity = true;
                SetCapture(window);
                g.settings.opacityPercent =
                    OpacityFromX(client.x, g.settings.uiScalePercent, g.dpi);
                Present();
                return 0;
            }

            const int fieldIndex = FieldIndexOf(hot);
            if (fieldIndex >= 0) {
                SetFocusedField(fieldIndex);
                HDC dc = g.surface.dc();
                if (dc != nullptr) {
                    const RenderModel current = BuildModel();
                    const std::size_t caret =
                        CaretIndexFromX(dc, current, fieldIndex, client.x);
                    g.fields[fieldIndex] =
                        MoveCaretTo(g.fields[fieldIndex], caret, false);
                }
                g.selectingText = true;
                SetCapture(window);
                Present();
                return 0;
            }

            if (g.focusedField >= 0) {
                SetFocusedField(-1);
                Present();
            }
            break;
        }

        case WM_LBUTTONDBLCLK: {
            POINT client = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const RenderModel model = BuildModel();
            const int fieldIndex = FieldIndexOf(HitTest(client, model));
            if (fieldIndex >= 0) {
                SetFocusedField(fieldIndex);
                g.fields[fieldIndex] = SelectAll(g.fields[fieldIndex]);
                Present();
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            POINT client = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            if (g.resizing) {
                EndResize();
                return 0;
            }

            if (g.adjustingOpacity) {
                g.settings.opacityPercent =
                    OpacityFromX(client.x, g.settings.uiScalePercent, g.dpi);
                g.adjustingOpacity = false;
                if (GetCapture() == window) {
                    ReleaseCapture();
                }
                SaveSettings(g.settingsPath, g.settings);
                Present();
                return 0;
            }

            if (g.selectingText) {
                g.selectingText = false;
                if (GetCapture() == window) {
                    ReleaseCapture();
                }
                return 0;
            }

            const RenderModel model = BuildModel();
            switch (HitTest(client, model)) {
                case HotElement::Pin:
                    TogglePinned();
                    break;
                case HotElement::Close:
                    PostMessageW(window, WM_CLOSE, 0, 0);
                    break;
                case HotElement::Settings:
                    SetExpanded(!g.expanded);
                    break;
                case HotElement::Autostart:
                    ToggleAutostart();
                    break;
                case HotElement::Save:
                    ApplySave();
                    break;
                default:
                    break;
            }
            return 0;
        }

        case WM_CAPTURECHANGED:
            if (g.resizing) {
                g.resizing = false;
                SaveSettings(g.settingsPath, g.settings);
            }
            if (g.adjustingOpacity) {
                g.adjustingOpacity = false;
                SaveSettings(g.settingsPath, g.settings);
            }
            g.selectingText = false;
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_TAB) {
                const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                FocusNextField(shift ? -1 : 1);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                if (g.resizing) {
                    EndResize();
                } else if (g.focusedField >= 0) {
                    SetFocusedField(-1);
                    Present();
                }
                return 0;
            }
            if (wParam == VK_RETURN && g.expanded) {
                ApplySave();
                return 0;
            }
            if (HandleFieldKey(wParam)) {
                return 0;
            }
            break;

        case WM_CHAR: {
            const auto character = static_cast<wchar_t>(wParam);
            if (character < 0x20 || g.focusedField < 0) {
                break;
            }
            const int index = g.focusedField;
            g.fields[index] = InsertText(
                g.fields[index], std::wstring_view(&character, 1),
                FilterFor(index), MaxLengthFor(index));
            if (g.footerIsError) {
                ResetFooter();
            }
            g.caretVisible = true;
            SetTimer(window, kCaretTimer, kCaretBlinkMs, nullptr);
            Present();
            return 0;
        }

        case WM_ACTIVATE:
            EnsureTopmost();
            return 0;

        case WM_DPICHANGED: {
            g.dpi = HIWORD(wParam);
            EnsureFonts(0);  // force a rebuild at the new DPI
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            PresentAt({suggested->left, suggested->top}, g.settings.uiScalePercent);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            const SIZE smallest = WindowSizeFor(kMinScalePercent, g.dpi, g.expanded);
            const SIZE largest = WindowSizeFor(kMaxScalePercent, g.dpi, g.expanded);
            info->ptMinTrackSize = {smallest.cx, smallest.cy};
            info->ptMaxTrackSize = {largest.cx, largest.cy};
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, kClockTimer);
            KillTimer(window, kCaretTimer);
            DestroyFonts(g.fonts);
            if (g.icon != nullptr) {
                DestroyIcon(g.icon);
                g.icon = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int RunApp(HINSTANCE instance, int showCommand) {
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    HICON appIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_OFFWORK));
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = appIcon;
    windowClass.hIconSm = appIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClass;

    if (RegisterClassExW(&windowClass) == 0) {
        return 1;
    }

    g.dpi = GetDpiForSystem();
    g.settingsPath = SettingsFilePath();
    g.settings = LoadSettings(g.settingsPath);

    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const SIZE size = WindowSizeFor(g.settings.uiScalePercent, g.dpi, false);
    const int margin = ScaleValue(20, g.settings.uiScalePercent, g.dpi);

    HWND window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kWindowClass, kWindowTitle,
        // No WS_THICKFRAME: it added a 7px invisible non-client border on every
        // edge, so the client area was 346x316 inside a 360x330 window. Every
        // hit test was offset by that much, and the frame itself showed up as
        // the unexplained white outline T006 recorded. Resizing is handled by
        // this window anyway, so the style bought nothing.
        WS_POPUP,
        workArea.right - size.cx - margin,
        workArea.top + margin,
        size.cx, size.cy,
        nullptr, nullptr, instance, nullptr);

    if (window == nullptr) {
        UnregisterClassW(kWindowClass, instance);
        return 1;
    }

    // Publish a frame before the window is shown, so it never appears blank.
    Present();
    ShowWindow(window, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
    Present();
    EnsureTopmost();

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnregisterClassW(kWindowClass, instance);
    return static_cast<int>(message.wParam);
}

}  // namespace offwork
