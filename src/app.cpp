#include "app.h"

#include "app_edit.h"
#include "app_state.h"
#include "app_view.h"
#include "autostart.h"
#include "render.h"
#include "resource.h"
#include "settings.h"
#include "theme.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace offwork {
namespace {

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
