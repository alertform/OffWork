#include "calculator.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"OffWork.Native.Widget";
constexpr wchar_t kWindowTitle[] = L"OffWork";
constexpr wchar_t kMutexName[] = L"Local\\OffWork.Native.SingleInstance";
constexpr UINT_PTR kClockTimer = 1;
constexpr int kStartEditId = 1001;
constexpr int kEndEditId = 1002;
constexpr int kSalaryEditId = 1003;
constexpr int kWindowWidth = 360;
constexpr int kCollapsedHeight = 330;
constexpr int kExpandedHeight = 690;
constexpr int kMinScalePercent = 75;
constexpr int kMaxScalePercent = 150;
constexpr BYTE kTranslucentAlpha = 217;

constexpr COLORREF kBackground = RGB(4, 17, 35);
constexpr COLORREF kTitleBackground = RGB(18, 29, 44);
constexpr COLORREF kPanelBackground = RGB(15, 29, 48);
constexpr COLORREF kInputBackground = RGB(27, 43, 65);
constexpr COLORREF kButtonBackground = RGB(21, 35, 56);
constexpr COLORREF kButtonHover = RGB(32, 51, 77);
constexpr COLORREF kAccent = RGB(79, 191, 244);
constexpr COLORREF kAccentHover = RGB(102, 205, 250);
constexpr COLORREF kTextPrimary = RGB(244, 248, 253);
constexpr COLORREF kTextSecondary = RGB(188, 202, 220);
constexpr COLORREF kTextMuted = RGB(139, 156, 178);
constexpr COLORREF kDanger = RGB(255, 126, 142);
constexpr COLORREF kProgressTrack = RGB(135, 153, 176);

struct Settings {
    int startMinutes = 9 * 60;
    int endMinutes = 18 * 60;
    double dailySalary = 500.0;
    int uiScalePercent = 100;
    bool translucent = false;
};

enum class HotElement {
    None,
    Pin,
    Close,
    Settings,
    Transparency,
    Save,
};

struct AppState {
    HWND window = nullptr;
    HWND startEdit = nullptr;
    HWND endEdit = nullptr;
    HWND salaryEdit = nullptr;
    HICON icon = nullptr;
    HFONT titleFont = nullptr;
    HFONT tinyFont = nullptr;
    HFONT labelFont = nullptr;
    HFONT countdownFont = nullptr;
    HFONT amountFont = nullptr;
    HFONT buttonFont = nullptr;
    HBRUSH editBrush = nullptr;
    UINT dpi = 96;
    bool pinned = true;
    bool expanded = false;
    bool validationError = false;
    HotElement hot = HotElement::None;
    Settings settings;
    std::wstring settingsPath;
};

AppState g;

int ScaleForDpi(int value) {
    return MulDiv(value, static_cast<int>(g.dpi), 96);
}

int Scale(int value) {
    return MulDiv(ScaleForDpi(value), g.settings.uiScalePercent, 100);
}

int Unscale(int value) {
    const int dpiValue = MulDiv(value, 100, g.settings.uiScalePercent);
    return MulDiv(dpiValue, 96, static_cast<int>(g.dpi));
}

RECT ScaledRect(int left, int top, int right, int bottom) {
    return {Scale(left), Scale(top), Scale(right), Scale(bottom)};
}

bool Contains(const RECT& rect, POINT point) {
    return PtInRect(&rect, point) != FALSE;
}

void DestroyFontHandle(HFONT& font) {
    if (font != nullptr) {
        DeleteObject(font);
        font = nullptr;
    }
}

HFONT MakeFont(int pixelSize, int weight, const wchar_t* family) {
    return CreateFontW(
        -Scale(pixelSize), 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
}

void CreateFonts() {
    DestroyFontHandle(g.titleFont);
    DestroyFontHandle(g.tinyFont);
    DestroyFontHandle(g.labelFont);
    DestroyFontHandle(g.countdownFont);
    DestroyFontHandle(g.amountFont);
    DestroyFontHandle(g.buttonFont);

    g.titleFont = MakeFont(16, FW_SEMIBOLD, L"Microsoft YaHei UI");
    g.tinyFont = MakeFont(11, FW_NORMAL, L"Microsoft YaHei UI");
    g.labelFont = MakeFont(15, FW_NORMAL, L"Microsoft YaHei UI");
    g.countdownFont = MakeFont(36, FW_SEMIBOLD, L"Bahnschrift");
    g.amountFont = MakeFont(30, FW_SEMIBOLD, L"Bahnschrift");
    g.buttonFont = MakeFont(14, FW_SEMIBOLD, L"Microsoft YaHei UI");

    if (g.startEdit != nullptr) {
        SendMessageW(g.startEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g.labelFont), TRUE);
        SendMessageW(g.endEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g.labelFont), TRUE);
        SendMessageW(g.salaryEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g.labelFont), TRUE);
    }
}

void FillSolid(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void FillRounded(HDC dc, const RECT& rect, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_NULL, 0, color);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, Scale(radius), Scale(radius));
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void FillEllipse(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_NULL, 0, color);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, pen);
    Ellipse(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawTextInRect(
    HDC dc,
    const std::wstring& text,
    RECT rect,
    HFONT font,
    COLORREF color,
    UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    const auto oldFont = SelectObject(dc, font);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
    SelectObject(dc, oldFont);
}

void DrawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, std::max(1, Scale(width)), color);
    const auto oldPen = SelectObject(dc, pen);
    MoveToEx(dc, Scale(x1), Scale(y1), nullptr);
    LineTo(dc, Scale(x2), Scale(y2));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

std::wstring SettingsFilePath() {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData))) {
        return L"OffWork.ini";
    }

    std::filesystem::path directory(localAppData);
    CoTaskMemFree(localAppData);
    directory /= L"OffWork";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return (directory / L"settings.ini").wstring();
}

Settings LoadSettings() {
    Settings settings;
    g.settingsPath = SettingsFilePath();
    settings.startMinutes = GetPrivateProfileIntW(
        L"work", L"startMinutes", settings.startMinutes, g.settingsPath.c_str());
    settings.endMinutes = GetPrivateProfileIntW(
        L"work", L"endMinutes", settings.endMinutes, g.settingsPath.c_str());
    settings.uiScalePercent = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(
            L"window", L"scalePercent", settings.uiScalePercent, g.settingsPath.c_str())),
        kMinScalePercent, kMaxScalePercent);
    settings.translucent = GetPrivateProfileIntW(
        L"window", L"translucent", settings.translucent ? 1 : 0,
        g.settingsPath.c_str()) != 0;

    wchar_t salary[64] = {};
    GetPrivateProfileStringW(
        L"work", L"dailySalary", L"500", salary,
        static_cast<DWORD>(std::size(salary)), g.settingsPath.c_str());
    wchar_t* end = nullptr;
    const double parsedSalary = std::wcstod(salary, &end);
    if (end != salary && parsedSalary >= 0.0) {
        settings.dailySalary = parsedSalary;
    }

    if (settings.startMinutes < 0 || settings.startMinutes >= 24 * 60 ||
        settings.endMinutes < 0 || settings.endMinutes >= 24 * 60 ||
        settings.startMinutes == settings.endMinutes) {
        settings.startMinutes = 9 * 60;
        settings.endMinutes = 18 * 60;
    }
    return settings;
}

void SaveSettings() {
    wchar_t value[64] = {};
    swprintf_s(value, L"%d", g.settings.startMinutes);
    WritePrivateProfileStringW(L"work", L"startMinutes", value, g.settingsPath.c_str());
    swprintf_s(value, L"%d", g.settings.endMinutes);
    WritePrivateProfileStringW(L"work", L"endMinutes", value, g.settingsPath.c_str());
    swprintf_s(value, L"%.2f", g.settings.dailySalary);
    WritePrivateProfileStringW(L"work", L"dailySalary", value, g.settingsPath.c_str());
    swprintf_s(value, L"%d", g.settings.uiScalePercent);
    WritePrivateProfileStringW(L"window", L"scalePercent", value, g.settingsPath.c_str());
    WritePrivateProfileStringW(
        L"window", L"translucent", g.settings.translucent ? L"1" : L"0",
        g.settingsPath.c_str());
}

std::wstring FormatTime(int minutes) {
    wchar_t text[16] = {};
    swprintf_s(text, L"%02d:%02d", minutes / 60, minutes % 60);
    return text;
}

void PopulateEditors() {
    SetWindowTextW(g.startEdit, FormatTime(g.settings.startMinutes).c_str());
    SetWindowTextW(g.endEdit, FormatTime(g.settings.endMinutes).c_str());
    wchar_t salary[64] = {};
    swprintf_s(salary, L"%.2f", g.settings.dailySalary);
    std::wstring salaryText(salary);
    while (salaryText.size() > 1 && salaryText.back() == L'0') {
        salaryText.pop_back();
    }
    if (!salaryText.empty() && salaryText.back() == L'.') {
        salaryText.pop_back();
    }
    SetWindowTextW(g.salaryEdit, salaryText.c_str());
}

void ArrangeEditors() {
    const RECT editRects[] = {
        ScaledRect(45, 350, 315, 374),
        ScaledRect(45, 417, 315, 441),
        ScaledRect(45, 484, 315, 508),
    };
    HWND edits[] = {g.startEdit, g.endEdit, g.salaryEdit};
    for (int index = 0; index < 3; ++index) {
        const RECT& rect = editRects[index];
        SetWindowPos(
            edits[index], nullptr, rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void ShowEditors(bool visible) {
    const int command = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(g.startEdit, command);
    ShowWindow(g.endEdit, command);
    ShowWindow(g.salaryEdit, command);
}

void ResizeWindow(int logicalHeight) {
    SetWindowPos(
        g.window, nullptr, 0, 0, Scale(kWindowWidth), Scale(logicalHeight),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void SetExpanded(bool expanded) {
    if (g.expanded == expanded) {
        return;
    }

    if (expanded) {
        g.expanded = true;
        g.validationError = false;
        ResizeWindow(kExpandedHeight);
        ArrangeEditors();
        ShowEditors(true);
    } else {
        // Hide child HWNDs and synchronously paint the collapsed state while
        // the backing surface is still full-sized. The following single resize
        // then crops an already-correct frame instead of briefly exposing the
        // expanded panel from the compositor's previous frame.
        ShowEditors(false);
        g.expanded = false;
        g.validationError = false;
        RedrawWindow(
            g.window, nullptr, nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        ResizeWindow(kCollapsedHeight);
    }

    InvalidateRect(g.window, nullptr, FALSE);
    UpdateWindow(g.window);
}

bool ParseTimeText(const wchar_t* text, int& minutes) {
    int hour = -1;
    int minute = -1;
    wchar_t trailing = L'\0';
    if (swscanf_s(text, L"%d:%d %c", &hour, &minute, &trailing, 1) != 2 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    minutes = hour * 60 + minute;
    return true;
}

bool ReadEditorSettings(Settings& settings) {
    settings = g.settings;
    wchar_t start[32] = {};
    wchar_t end[32] = {};
    wchar_t salary[64] = {};
    GetWindowTextW(g.startEdit, start, static_cast<int>(std::size(start)));
    GetWindowTextW(g.endEdit, end, static_cast<int>(std::size(end)));
    GetWindowTextW(g.salaryEdit, salary, static_cast<int>(std::size(salary)));

    if (!ParseTimeText(start, settings.startMinutes) ||
        !ParseTimeText(end, settings.endMinutes) ||
        settings.startMinutes == settings.endMinutes) {
        return false;
    }

    wchar_t* salaryEnd = nullptr;
    settings.dailySalary = std::wcstod(salary, &salaryEnd);
    while (salaryEnd != nullptr && *salaryEnd == L' ') {
        ++salaryEnd;
    }
    return salaryEnd != salary && salaryEnd != nullptr && *salaryEnd == L'\0' &&
        std::isfinite(settings.dailySalary) && settings.dailySalary >= 0.0 &&
        settings.dailySalary <= 1000000.0;
}

void EnsureTopmost() {
    if (g.pinned) {
        SetWindowPos(
            g.window, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void TogglePinned() {
    g.pinned = !g.pinned;
    SetWindowPos(
        g.window, g.pinned ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    InvalidateRect(g.window, nullptr, FALSE);
}

void ApplyTransparency() {
    SetLayeredWindowAttributes(
        g.window, 0, g.settings.translucent ? kTranslucentAlpha : 255, LWA_ALPHA);
}

RECT PinRect() {
    return ScaledRect(272, 12, 304, 44);
}

RECT CloseRect() {
    return ScaledRect(316, 12, 348, 44);
}

RECT SettingsRect() {
    return ScaledRect(20, 258, 340, 304);
}

RECT TransparencyRect() {
    return ScaledRect(268, 530, 323, 558);
}

RECT SaveRect() {
    return ScaledRect(37, 584, 323, 624);
}

HotElement HitElement(POINT point) {
    if (Contains(PinRect(), point)) {
        return HotElement::Pin;
    }
    if (Contains(CloseRect(), point)) {
        return HotElement::Close;
    }
    if (Contains(SettingsRect(), point)) {
        return HotElement::Settings;
    }
    if (g.expanded && Contains(TransparencyRect(), point)) {
        return HotElement::Transparency;
    }
    if (g.expanded && Contains(SaveRect(), point)) {
        return HotElement::Save;
    }
    return HotElement::None;
}

void DrawPin(HDC dc, const RECT& rect) {
    const int left = Unscale(rect.left);
    const int top = Unscale(rect.top);
    const COLORREF color = g.pinned ? RGB(6, 28, 43) : kTextPrimary;
    DrawLine(dc, left + 11, top + 9, left + 21, top + 9, color, 2);
    DrawLine(dc, left + 13, top + 9, left + 13, top + 16, color, 1);
    DrawLine(dc, left + 19, top + 9, left + 19, top + 16, color, 1);
    DrawLine(dc, left + 10, top + 16, left + 22, top + 16, color, 2);
    DrawLine(dc, left + 16, top + 16, left + 16, top + 24, color, 1);
}

void DrawClose(HDC dc, const RECT& rect) {
    const int left = Unscale(rect.left);
    const int top = Unscale(rect.top);
    DrawLine(dc, left + 11, top + 11, left + 21, top + 21, kTextPrimary, 1);
    DrawLine(dc, left + 21, top + 11, left + 11, top + 21, kTextPrimary, 1);
}

void DrawChevron(HDC dc, bool expanded) {
    const int centerX = 318;
    const int centerY = 281;
    if (expanded) {
        DrawLine(dc, centerX - 4, centerY + 2, centerX, centerY - 2, kTextSecondary, 1);
        DrawLine(dc, centerX, centerY - 2, centerX + 4, centerY + 2, kTextSecondary, 1);
    } else {
        DrawLine(dc, centerX - 4, centerY - 2, centerX, centerY + 2, kTextSecondary, 1);
        DrawLine(dc, centerX, centerY + 2, centerX + 4, centerY - 2, kTextSecondary, 1);
    }
}

void DrawTransparencyToggle(HDC dc) {
    const RECT track = TransparencyRect();
    FillRounded(
        dc, track, 14,
        g.settings.translucent
            ? (g.hot == HotElement::Transparency ? kAccentHover : kAccent)
            : (g.hot == HotElement::Transparency ? kButtonHover : kButtonBackground));
    const RECT thumb = g.settings.translucent
        ? ScaledRect(297, 534, 319, 556)
        : ScaledRect(272, 534, 294, 556);
    FillEllipse(
        dc, thumb,
        g.settings.translucent ? RGB(4, 34, 51) : kTextSecondary);
}

void DrawResizeGrip(HDC dc) {
    const int bottom = g.expanded ? kExpandedHeight : kCollapsedHeight;
    DrawLine(dc, 346, bottom - 7, 353, bottom - 14, kTextMuted, 1);
    DrawLine(dc, 340, bottom - 7, 353, bottom - 20, kTextMuted, 1);
}

void PaintWindow(HDC target, const RECT& client) {
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    const auto oldBitmap = SelectObject(dc, bitmap);

    FillSolid(dc, client, kBackground);
    FillSolid(dc, ScaledRect(0, 0, kWindowWidth, 56), kTitleBackground);

    DrawIconEx(dc, Scale(16), Scale(12), g.icon, Scale(32), Scale(32), 0, nullptr, DI_NORMAL);
    DrawTextInRect(dc, L"OffWork", ScaledRect(56, 7, 220, 29), g.titleFont, kTextPrimary);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    wchar_t dateTime[64] = {};
    swprintf_s(
        dateTime, L"%d月%d日  %02d:%02d:%02d",
        time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    DrawTextInRect(dc, dateTime, ScaledRect(56, 27, 235, 46), g.tinyFont, kTextSecondary);

    const RECT pinRect = PinRect();
    const COLORREF pinColor = g.pinned
        ? (g.hot == HotElement::Pin ? kAccentHover : kAccent)
        : (g.hot == HotElement::Pin ? kButtonHover : kButtonBackground);
    FillRounded(dc, pinRect, 8, pinColor);
    DrawPin(dc, pinRect);

    const RECT closeRect = CloseRect();
    if (g.hot == HotElement::Close) {
        FillRounded(dc, closeRect, 8, RGB(196, 43, 58));
    }
    DrawClose(dc, closeRect);

    const int nowSeconds = time.wHour * 3600 + time.wMinute * 60 + time.wSecond;
    const auto snapshot = offwork::CalculateWorkday(
        nowSeconds, g.settings.startMinutes, g.settings.endMinutes, g.settings.dailySalary);

    std::wstring status;
    std::wstring countdown;
    if (snapshot.phase == offwork::WorkdayPhase::BeforeWork) {
        status = L"距离上班";
    } else if (snapshot.phase == offwork::WorkdayPhase::Working) {
        status = L"距离下班";
    } else {
        status = L"今天的工作完成啦";
        countdown = L"已下班  :)";
    }

    if (countdown.empty()) {
        wchar_t duration[32] = {};
        const int hours = snapshot.remainingSeconds / 3600;
        const int minutes = (snapshot.remainingSeconds % 3600) / 60;
        const int seconds = snapshot.remainingSeconds % 60;
        swprintf_s(duration, L"%02d:%02d:%02d", hours, minutes, seconds);
        countdown = duration;
    }

    DrawTextInRect(dc, status, ScaledRect(20, 68, 260, 88), g.labelFont, kTextSecondary);
    DrawTextInRect(dc, countdown, ScaledRect(20, 87, 340, 132), g.countdownFont, kTextPrimary);

    FillRounded(dc, ScaledRect(20, 144, 340, 147), 2, kProgressTrack);
    const int progressRight = 20 + static_cast<int>(320.0 * snapshot.progress);
    if (progressRight > 20) {
        FillRounded(dc, ScaledRect(20, 144, progressRight, 147), 2, kAccent);
    }
    wchar_t progress[48] = {};
    swprintf_s(progress, L"今日进度 %.0f%%", snapshot.progress * 100.0);
    DrawTextInRect(
        dc, progress, ScaledRect(180, 152, 340, 174), g.tinyFont, kTextSecondary,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DrawTextInRect(dc, L"今天已经赚了", ScaledRect(20, 184, 200, 204), g.labelFont, kTextSecondary);
    wchar_t earned[64] = {};
    swprintf_s(earned, L"¥%.2f", snapshot.earned);
    DrawTextInRect(dc, earned, ScaledRect(20, 204, 250, 244), g.amountFont, RGB(127, 224, 250));
    wchar_t salary[64] = {};
    swprintf_s(salary, L"日薪 ¥%.2f", g.settings.dailySalary);
    DrawTextInRect(
        dc, salary, ScaledRect(220, 217, 340, 241), g.tinyFont, kTextSecondary,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const COLORREF settingsColor = g.hot == HotElement::Settings ? kButtonHover : kButtonBackground;
    FillRounded(dc, SettingsRect(), 6, settingsColor);
    DrawTextInRect(dc, L"设置", ScaledRect(36, 258, 130, 304), g.buttonFont, kTextPrimary);
    DrawChevron(dc, g.expanded);

    if (g.expanded) {
        FillRounded(dc, ScaledRect(20, 306, 340, 672), 6, kPanelBackground);
        DrawTextInRect(dc, L"上班时间", ScaledRect(37, 315, 220, 339), g.labelFont, kTextPrimary);
        DrawTextInRect(dc, L"下班时间", ScaledRect(37, 382, 220, 406), g.labelFont, kTextPrimary);
        DrawTextInRect(dc, L"日薪（元）", ScaledRect(37, 449, 220, 473), g.labelFont, kTextPrimary);

        FillRounded(dc, ScaledRect(37, 344, 323, 380), 5, kInputBackground);
        FillRounded(dc, ScaledRect(37, 411, 323, 447), 5, kInputBackground);
        FillRounded(dc, ScaledRect(37, 478, 323, 514), 5, kInputBackground);

        DrawTextInRect(
            dc, L"半透明（85%）", ScaledRect(37, 530, 250, 558),
            g.labelFont, kTextPrimary);
        DrawTransparencyToggle(dc);

        const COLORREF saveColor = g.hot == HotElement::Save ? kAccentHover : kAccent;
        FillRounded(dc, SaveRect(), 6, saveColor);
        DrawTextInRect(
            dc, L"保存设置", SaveRect(), g.buttonFont, RGB(4, 34, 51),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        if (g.validationError) {
            DrawTextInRect(
                dc, L"请输入 HH:MM 时间和有效日薪", ScaledRect(37, 636, 323, 657),
                g.tinyFont, kDanger, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        } else {
            DrawTextInRect(
                dc, L"设置和窗口大小保存在本机", ScaledRect(37, 636, 323, 657),
                g.tinyFont, kTextMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    DrawResizeGrip(dc);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void CreateEditors(HWND parent, HINSTANCE instance) {
    const DWORD style = WS_CHILD | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL;
    g.startEdit = CreateWindowExW(
        0, L"EDIT", L"09:00", style, 0, 0, 0, 0,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStartEditId)), instance, nullptr);
    g.endEdit = CreateWindowExW(
        0, L"EDIT", L"18:00", style, 0, 0, 0, 0,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEndEditId)), instance, nullptr);
    g.salaryEdit = CreateWindowExW(
        0, L"EDIT", L"500", style, 0, 0, 0, 0,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSalaryEditId)), instance, nullptr);

    HWND edits[] = {g.startEdit, g.endEdit, g.salaryEdit};
    for (HWND edit : edits) {
        SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
        SetWindowTheme(edit, L"DarkMode_Explorer", nullptr);
    }
    SendMessageW(g.startEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(g.endEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(g.salaryEdit, EM_SETLIMITTEXT, 16, 0);
    ArrangeEditors();
    ShowEditors(false);
}

void ApplyWindowAppearance(HWND window) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(window, 20, &darkMode, sizeof(darkMode));
    constexpr DWORD roundedPreference = 2;
    DwmSetWindowAttribute(window, 33, &roundedPreference, sizeof(roundedPreference));
}

void SetUiScale(int percent) {
    percent = std::clamp(percent, kMinScalePercent, kMaxScalePercent);
    if (g.settings.uiScalePercent == percent) {
        return;
    }
    g.settings.uiScalePercent = percent;
    CreateFonts();
    ArrangeEditors();
    if (g.window != nullptr) {
        InvalidateRect(g.window, nullptr, FALSE);
    }
}

void ConstrainSizingRect(RECT& rect, WPARAM edge) {
    const int baseWidth = ScaleForDpi(kWindowWidth);
    const int baseHeight = ScaleForDpi(g.expanded ? kExpandedHeight : kCollapsedHeight);
    const int widthPercent = MulDiv(rect.right - rect.left, 100, baseWidth);
    const int heightPercent = MulDiv(rect.bottom - rect.top, 100, baseHeight);

    int percent = widthPercent;
    if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) {
        percent = heightPercent;
    } else if (edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT ||
               edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT) {
        const int widthDelta = std::abs(widthPercent - g.settings.uiScalePercent);
        const int heightDelta = std::abs(heightPercent - g.settings.uiScalePercent);
        percent = heightDelta > widthDelta ? heightPercent : widthPercent;
    }
    percent = std::clamp(percent, kMinScalePercent, kMaxScalePercent);

    const int width = MulDiv(baseWidth, percent, 100);
    const int height = MulDiv(baseHeight, percent, 100);
    const bool resizeFromLeft = edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT ||
        edge == WMSZ_BOTTOMLEFT;
    const bool resizeFromTop = edge == WMSZ_TOP || edge == WMSZ_TOPLEFT ||
        edge == WMSZ_TOPRIGHT;

    if (resizeFromLeft) {
        rect.left = rect.right - width;
    } else {
        rect.right = rect.left + width;
    }
    if (resizeFromTop) {
        rect.top = rect.bottom - height;
    } else {
        rect.bottom = rect.top + height;
    }
    SetUiScale(percent);
}

LRESULT ResizeHitTest(HWND window, POINT point) {
    RECT client = {};
    GetClientRect(window, &client);
    const int border = std::max(4, Scale(6));
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
    return HTCLIENT;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g.window = window;
            g.dpi = GetDpiForWindow(window);
            g.icon = static_cast<HICON>(LoadImageW(
                GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_OFFWORK), IMAGE_ICON,
                Scale(32), Scale(32), LR_DEFAULTCOLOR));
            g.editBrush = CreateSolidBrush(kInputBackground);
            CreateEditors(window, reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance);
            CreateFonts();
            PopulateEditors();
            ApplyWindowAppearance(window);
            ApplyTransparency();
            SetTimer(window, kClockTimer, 1000, nullptr);
            return 0;
        }
        case WM_TIMER:
            if (wParam == kClockTimer) {
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            RECT client = {};
            GetClientRect(window, &client);
            PaintWindow(dc, client);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_NCCALCSIZE:
            if (wParam != 0) {
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(window, &point);
            const LRESULT resizeHit = ResizeHitTest(window, point);
            if (resizeHit != HTCLIENT) {
                return resizeHit;
            }
            if (point.y < Scale(56) && HitElement(point) == HotElement::None) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_MOUSEMOVE: {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const HotElement hot = HitElement(point);
            if (hot != g.hot) {
                g.hot = hot;
                InvalidateRect(window, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            return 0;
        }
        case WM_MOUSELEAVE:
            g.hot = HotElement::None;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            switch (HitElement(point)) {
                case HotElement::Pin:
                    TogglePinned();
                    break;
                case HotElement::Close:
                    PostMessageW(window, WM_CLOSE, 0, 0);
                    break;
                case HotElement::Settings:
                    SetExpanded(!g.expanded);
                    break;
                case HotElement::Transparency:
                    g.settings.translucent = !g.settings.translucent;
                    ApplyTransparency();
                    SaveSettings();
                    InvalidateRect(window, nullptr, FALSE);
                    break;
                case HotElement::Save: {
                    Settings settings;
                    if (ReadEditorSettings(settings)) {
                        g.settings = settings;
                        SaveSettings();
                        SetExpanded(false);
                    } else {
                        g.validationError = true;
                        InvalidateRect(window, nullptr, FALSE);
                    }
                    break;
                }
                case HotElement::None:
                    break;
            }
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && g.validationError) {
                g.validationError = false;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, kTextPrimary);
            SetBkColor(dc, kInputBackground);
            SetBkMode(dc, OPAQUE);
            return reinterpret_cast<LRESULT>(g.editBrush);
        }
        case WM_ACTIVATE:
            EnsureTopmost();
            return 0;
        case WM_SIZING:
            ConstrainSizingRect(*reinterpret_cast<RECT*>(lParam), wParam);
            return TRUE;
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                const int baseWidth = ScaleForDpi(kWindowWidth);
                if (baseWidth > 0) {
                    SetUiScale(MulDiv(LOWORD(lParam), 100, baseWidth));
                }
            }
            return 0;
        case WM_EXITSIZEMOVE:
            SaveSettings();
            return 0;
        case WM_DPICHANGED: {
            g.dpi = HIWORD(wParam);
            CreateFonts();
            ArrangeEditors();
            const auto suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(
                window, nullptr, suggested->left, suggested->top,
                Scale(kWindowWidth), Scale(g.expanded ? kExpandedHeight : kCollapsedHeight),
                SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            const int logicalHeight = g.expanded ? kExpandedHeight : kCollapsedHeight;
            info->ptMinTrackSize = {
                MulDiv(ScaleForDpi(kWindowWidth), kMinScalePercent, 100),
                MulDiv(ScaleForDpi(logicalHeight), kMinScalePercent, 100)};
            info->ptMaxTrackSize = {
                MulDiv(ScaleForDpi(kWindowWidth), kMaxScalePercent, 100),
                MulDiv(ScaleForDpi(logicalHeight), kMaxScalePercent, 100)};
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            KillTimer(window, kClockTimer);
            DestroyFontHandle(g.titleFont);
            DestroyFontHandle(g.tinyFont);
            DestroyFontHandle(g.labelFont);
            DestroyFontHandle(g.countdownFont);
            DestroyFontHandle(g.amountFont);
            DestroyFontHandle(g.buttonFont);
            if (g.editBrush != nullptr) {
                DeleteObject(g.editBrush);
                g.editBrush = nullptr;
            }
            if (g.icon != nullptr) {
                DestroyIcon(g.icon);
                g.icon = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(kWindowClass, kWindowTitle); existing != nullptr) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(mutex);
        return 0;
    }

    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    HICON appIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_OFFWORK));
    HBRUSH background = CreateSolidBrush(kBackground);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = appIcon;
    windowClass.hIconSm = appIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = background;
    windowClass.lpszClassName = kWindowClass;

    if (RegisterClassExW(&windowClass) == 0) {
        DeleteObject(background);
        CloseHandle(mutex);
        return 1;
    }

    g.dpi = GetDpiForSystem();
    g.settings = LoadSettings();
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int width = Scale(kWindowWidth);
    const int height = Scale(kCollapsedHeight);
    const int margin = Scale(20);

    HWND window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kWindowClass,
        kWindowTitle,
        WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN,
        workArea.right - width - margin,
        workArea.top + margin,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr) {
        UnregisterClassW(kWindowClass, instance);
        DeleteObject(background);
        CloseHandle(mutex);
        return 1;
    }

    ShowWindow(window, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(window);
    EnsureTopmost();

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    UnregisterClassW(kWindowClass, instance);
    DeleteObject(background);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
