#include "render.h"

#include "calculator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace offwork {
namespace {

int ScaleForDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

RECT ToRect(const LogicalRect& rect, int scalePercent, UINT dpi) {
    return ScaleRect(rect, scalePercent, dpi);
}

bool Contains(const RECT& rect, POINT point) {
    return PtInRect(&rect, point) != FALSE;
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
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void StrokeRounded(HDC dc, const RECT& rect, int radius, COLORREF color, int width) {
    HPEN pen = CreatePen(PS_SOLID, std::max(1, width), color);
    const auto oldPen = SelectObject(dc, pen);
    const auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
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

void DrawScaledLine(
    HDC dc, int x1, int y1, int x2, int y2, COLORREF color,
    int width, int scalePercent, UINT dpi) {
    HPEN pen = CreatePen(
        PS_SOLID, std::max(1, ScaleValue(width, scalePercent, dpi)), color);
    const auto oldPen = SelectObject(dc, pen);
    MoveToEx(dc, ScaleValue(x1, scalePercent, dpi), ScaleValue(y1, scalePercent, dpi), nullptr);
    LineTo(dc, ScaleValue(x2, scalePercent, dpi), ScaleValue(y2, scalePercent, dpi));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

HFONT MakeFont(int pixelSize, int weight, const wchar_t* family, int scalePercent, UINT dpi) {
    return CreateFontW(
        -ScaleValue(pixelSize, scalePercent, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
}

std::wstring FormatTime(int minutes) {
    wchar_t text[16] = {};
    swprintf_s(text, L"%02d:%02d", minutes / 60, minutes % 60);
    return text;
}

const FieldState& FieldFor(const RenderModel& model, int index) {
    switch (index) {
        case 0: return model.startField;
        case 1: return model.endField;
        default: return model.salaryField;
    }
}

int TextWidth(HDC dc, const std::wstring& text) {
    if (text.empty()) {
        return 0;
    }
    SIZE size = {};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    return size.cx;
}

// Field text is centred, matching the ES_CENTER edit controls it replaced.
int FieldTextLeft(HDC dc, const RenderModel& model, int fieldIndex, const std::wstring& text) {
    const RECT box = ToRect(FieldBox(fieldIndex), model.settings.uiScalePercent, model.dpi);
    const int width = TextWidth(dc, text);
    return (box.left + box.right) / 2 - width / 2;
}

void DrawPin(HDC dc, const RenderModel& model) {
    const int left = kPinButton.left;
    const int top = kPinButton.top;
    const COLORREF color = model.pinned ? RGB(6, 28, 43) : kTextPrimary;
    const int scale = model.settings.uiScalePercent;
    DrawScaledLine(dc, left + 11, top + 9, left + 21, top + 9, color, 2, scale, model.dpi);
    DrawScaledLine(dc, left + 13, top + 9, left + 13, top + 16, color, 1, scale, model.dpi);
    DrawScaledLine(dc, left + 19, top + 9, left + 19, top + 16, color, 1, scale, model.dpi);
    DrawScaledLine(dc, left + 10, top + 16, left + 22, top + 16, color, 2, scale, model.dpi);
    DrawScaledLine(dc, left + 16, top + 16, left + 16, top + 24, color, 1, scale, model.dpi);
}

void DrawClose(HDC dc, const RenderModel& model) {
    const int left = kCloseButton.left;
    const int top = kCloseButton.top;
    const int scale = model.settings.uiScalePercent;
    DrawScaledLine(dc, left + 11, top + 11, left + 21, top + 21, kTextPrimary, 1, scale, model.dpi);
    DrawScaledLine(dc, left + 21, top + 11, left + 11, top + 21, kTextPrimary, 1, scale, model.dpi);
}

void DrawChevron(HDC dc, const RenderModel& model) {
    const int x = kChevronCenterX;
    const int y = kChevronCenterY;
    const int scale = model.settings.uiScalePercent;
    if (model.expanded) {
        DrawScaledLine(dc, x - 4, y + 2, x, y - 2, kTextSecondary, 1, scale, model.dpi);
        DrawScaledLine(dc, x, y - 2, x + 4, y + 2, kTextSecondary, 1, scale, model.dpi);
    } else {
        DrawScaledLine(dc, x - 4, y - 2, x, y + 2, kTextSecondary, 1, scale, model.dpi);
        DrawScaledLine(dc, x, y + 2, x + 4, y - 2, kTextSecondary, 1, scale, model.dpi);
    }
}

void DrawOpacitySlider(HDC dc, const RenderModel& model) {
    const int scale = model.settings.uiScalePercent;
    const RECT track = ToRect(kOpacityTrack, scale, model.dpi);
    FillRounded(dc, track, ScaleValue(3, scale, model.dpi), kProgressTrack);

    const int thumbCenter = track.left + MulDiv(
        model.settings.opacityPercent - kMinOpacityPercent,
        track.right - track.left, 100 - kMinOpacityPercent);

    RECT filled = track;
    filled.right = thumbCenter;
    if (filled.right > filled.left) {
        FillRounded(dc, filled, ScaleValue(3, scale, model.dpi), kAccent);
    }

    const int radius = ScaleValue(8, scale, model.dpi);
    const int centerY = (track.top + track.bottom) / 2;
    const RECT thumb = {
        thumbCenter - radius, centerY - radius,
        thumbCenter + radius, centerY + radius};
    FillEllipse(
        dc, thumb,
        model.hot == HotElement::Opacity ? kAccentHover : kTextPrimary);
}

void DrawAutostartToggle(HDC dc, const RenderModel& model) {
    const int scale = model.settings.uiScalePercent;
    const RECT track = ToRect(kAutostartToggle, scale, model.dpi);
    const int height = track.bottom - track.top;
    const int radius = height / 2;

    COLORREF trackColor = model.autostartEnabled ? kAccent : kToggleOff;
    if (model.hot == HotElement::Autostart) {
        trackColor = model.autostartEnabled ? kAccentHover : kButtonHover;
    }
    FillRounded(dc, track, height, trackColor);

    const int inset = std::max(2, ScaleValue(2, scale, model.dpi));
    const int knobDiameter = height - inset * 2;
    const int knobLeft = model.autostartEnabled
        ? track.right - inset - knobDiameter
        : track.left + inset;
    const RECT knob = {
        knobLeft, track.top + inset,
        knobLeft + knobDiameter, track.bottom - inset};
    FillEllipse(dc, knob, kTextPrimary);
    (void)radius;
}

void DrawField(HDC dc, const RenderModel& model, int fieldIndex) {
    const int scale = model.settings.uiScalePercent;
    const RECT box = ToRect(FieldBox(fieldIndex), scale, model.dpi);
    const int radius = ScaleValue(5, scale, model.dpi);
    FillRounded(dc, box, radius, kInputBackground);

    const bool focused = model.focusedField == fieldIndex;
    if (focused) {
        StrokeRounded(dc, box, radius, kInputBorderFocused,
                      ScaleValue(1, scale, model.dpi));
    }

    const FieldState& field = FieldFor(model, fieldIndex);
    const auto oldFont = SelectObject(dc, model.fonts->label);

    const int textLeft = FieldTextLeft(dc, model, fieldIndex, field.text);
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    const int textTop = (box.top + box.bottom) / 2 - metrics.tmHeight / 2;

    if (focused && HasSelection(field)) {
        const std::size_t begin = SelectionBegin(field);
        const std::size_t end = SelectionEnd(field);
        const int selectionLeft =
            textLeft + TextWidth(dc, field.text.substr(0, begin));
        const int selectionRight =
            textLeft + TextWidth(dc, field.text.substr(0, end));
        const RECT selection = {
            selectionLeft, box.top + ScaleValue(5, scale, model.dpi),
            selectionRight, box.bottom - ScaleValue(5, scale, model.dpi)};
        FillSolid(dc, selection, kSelectionBackground);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kTextPrimary);
    TextOutW(dc, textLeft, textTop, field.text.c_str(),
             static_cast<int>(field.text.size()));

    if (focused && model.caretVisible && !HasSelection(field)) {
        const int caretX =
            textLeft + TextWidth(dc, field.text.substr(0, field.caret));
        const RECT caret = {
            caretX, box.top + ScaleValue(6, scale, model.dpi),
            caretX + std::max(1, ScaleValue(1, scale, model.dpi)),
            box.bottom - ScaleValue(6, scale, model.dpi)};
        FillSolid(dc, caret, kTextPrimary);
    }

    SelectObject(dc, oldFont);
}

}  // namespace

// -- geometry ---------------------------------------------------------------

int ScaleValue(int logical, int scalePercent, UINT dpi) {
    return MulDiv(ScaleForDpi(logical, dpi), scalePercent, 100);
}

int UnscaleValue(int device, int scalePercent, UINT dpi) {
    const int atDpi = MulDiv(device, 100, scalePercent);
    return MulDiv(atDpi, 96, static_cast<int>(dpi));
}

RECT ScaleRect(const LogicalRect& rect, int scalePercent, UINT dpi) {
    return {
        ScaleValue(rect.left, scalePercent, dpi),
        ScaleValue(rect.top, scalePercent, dpi),
        ScaleValue(rect.right, scalePercent, dpi),
        ScaleValue(rect.bottom, scalePercent, dpi)};
}

SIZE WindowSizeFor(int scalePercent, UINT dpi, bool expanded) {
    const int logicalHeight = expanded ? kExpandedHeight : kCollapsedHeight;
    return {
        ScaleValue(kWindowWidth, scalePercent, dpi),
        ScaleValue(logicalHeight, scalePercent, dpi)};
}

int ScaleFromWidth(int deviceWidth, UINT dpi) {
    const int baseWidth = ScaleForDpi(kWindowWidth, dpi);
    if (baseWidth <= 0) {
        return 100;
    }
    return std::clamp(
        MulDiv(deviceWidth, 100, baseWidth), kMinScalePercent, kMaxScalePercent);
}

// -- fonts ------------------------------------------------------------------

FontSet CreateFonts(int scalePercent, UINT dpi) {
    FontSet fonts;
    fonts.title = MakeFont(16, FW_SEMIBOLD, L"Microsoft YaHei UI", scalePercent, dpi);
    fonts.tiny = MakeFont(11, FW_NORMAL, L"Microsoft YaHei UI", scalePercent, dpi);
    fonts.label = MakeFont(15, FW_NORMAL, L"Microsoft YaHei UI", scalePercent, dpi);
    fonts.countdown = MakeFont(36, FW_SEMIBOLD, L"Bahnschrift", scalePercent, dpi);
    fonts.amount = MakeFont(30, FW_SEMIBOLD, L"Bahnschrift", scalePercent, dpi);
    fonts.button = MakeFont(14, FW_SEMIBOLD, L"Microsoft YaHei UI", scalePercent, dpi);
    return fonts;
}

void DestroyFonts(FontSet& fonts) {
    for (HFONT* font : {&fonts.title, &fonts.tiny, &fonts.label,
                        &fonts.countdown, &fonts.amount, &fonts.button}) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

// -- fields -----------------------------------------------------------------

int FieldIndexOf(HotElement element) {
    switch (element) {
        case HotElement::StartField: return 0;
        case HotElement::EndField: return 1;
        case HotElement::SalaryField: return 2;
        default: return -1;
    }
}

const LogicalRect& FieldBox(int fieldIndex) {
    switch (fieldIndex) {
        case 0: return kStartBox;
        case 1: return kEndBox;
        default: return kSalaryBox;
    }
}

std::size_t CaretIndexFromX(
    HDC dc, const RenderModel& model, int fieldIndex, int clientX) {
    const FieldState& field = FieldFor(model, fieldIndex);
    const auto oldFont = SelectObject(dc, model.fonts->label);
    const int textLeft = FieldTextLeft(dc, model, fieldIndex, field.text);

    std::size_t best = 0;
    int bestDistance = std::abs(clientX - textLeft);
    for (std::size_t index = 1; index <= field.text.size(); ++index) {
        const int x = textLeft + TextWidth(dc, field.text.substr(0, index));
        const int distance = std::abs(clientX - x);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }

    SelectObject(dc, oldFont);
    return best;
}

// -- hit testing ------------------------------------------------------------

HotElement HitTest(POINT point, const RenderModel& model) {
    const int scale = model.settings.uiScalePercent;
    const UINT dpi = model.dpi;

    if (Contains(ToRect(kPinButton, scale, dpi), point)) {
        return HotElement::Pin;
    }
    if (Contains(ToRect(kCloseButton, scale, dpi), point)) {
        return HotElement::Close;
    }
    if (Contains(ToRect(kSettingsButton, scale, dpi), point)) {
        return HotElement::Settings;
    }
    if (!model.expanded) {
        return HotElement::None;
    }
    if (Contains(ToRect(kStartBox, scale, dpi), point)) {
        return HotElement::StartField;
    }
    if (Contains(ToRect(kEndBox, scale, dpi), point)) {
        return HotElement::EndField;
    }
    if (Contains(ToRect(kSalaryBox, scale, dpi), point)) {
        return HotElement::SalaryField;
    }
    if (Contains(ToRect(kOpacityHit, scale, dpi), point)) {
        return HotElement::Opacity;
    }
    if (Contains(ToRect(kAutostartHit, scale, dpi), point)) {
        return HotElement::Autostart;
    }
    if (Contains(ToRect(kSaveButton, scale, dpi), point)) {
        return HotElement::Save;
    }
    return HotElement::None;
}

int OpacityFromX(int clientX, int scalePercent, UINT dpi) {
    const RECT track = ScaleRect(kOpacityTrack, scalePercent, dpi);
    if (track.right <= track.left) {
        return 100;
    }
    const int x = std::clamp(clientX, static_cast<int>(track.left),
                             static_cast<int>(track.right));
    return kMinOpacityPercent + MulDiv(
        x - track.left, 100 - kMinOpacityPercent, track.right - track.left);
}

// -- frame ------------------------------------------------------------------

void RenderFrame(HDC dc, const RenderModel& model) {
    if (model.fonts == nullptr) {
        return;
    }

    const int scale = model.settings.uiScalePercent;
    const UINT dpi = model.dpi;
    const SIZE size = WindowSizeFor(scale, dpi, model.expanded);
    const RECT client = {0, 0, size.cx, size.cy};

    FillSolid(dc, client, kBackground);
    FillSolid(dc, ToRect(kTitleBar, scale, dpi), kTitleBackground);

    if (model.icon != nullptr) {
        const RECT icon = ToRect(kIcon, scale, dpi);
        DrawIconEx(
            dc, icon.left, icon.top, model.icon,
            icon.right - icon.left, icon.bottom - icon.top, 0, nullptr, DI_NORMAL);
    }
    DrawTextInRect(dc, L"OffWork", ToRect(kTitleText, scale, dpi),
                   model.fonts->title, kTextPrimary);

    wchar_t dateTime[64] = {};
    swprintf_s(
        dateTime, L"%d月%d日  %02d:%02d:%02d",
        model.now.wMonth, model.now.wDay,
        model.now.wHour, model.now.wMinute, model.now.wSecond);
    DrawTextInRect(dc, dateTime, ToRect(kDateTimeText, scale, dpi),
                   model.fonts->tiny, kTextSecondary);

    const RECT pinRect = ToRect(kPinButton, scale, dpi);
    const COLORREF pinColor = model.pinned
        ? (model.hot == HotElement::Pin ? kAccentHover : kAccent)
        : (model.hot == HotElement::Pin ? kButtonHover : kButtonBackground);
    FillRounded(dc, pinRect, ScaleValue(8, scale, dpi), pinColor);
    DrawPin(dc, model);

    if (model.hot == HotElement::Close) {
        FillRounded(dc, ToRect(kCloseButton, scale, dpi),
                    ScaleValue(8, scale, dpi), RGB(196, 43, 58));
    }
    DrawClose(dc, model);

    const int nowSeconds =
        model.now.wHour * 3600 + model.now.wMinute * 60 + model.now.wSecond;
    offwork::WorkdaySnapshot snapshot{
        offwork::WorkdayPhase::Working, 0, 0.0, 0.0};
    try {
        snapshot = offwork::CalculateWorkday(
            std::clamp(nowSeconds, 0, 24 * 60 * 60 - 1),
            model.settings.startMinutes, model.settings.endMinutes,
            model.settings.dailySalary);
    } catch (...) {
        // NormalizeSettings makes this unreachable; a painted frame must never
        // be the thing that takes the process down.
    }

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
        swprintf_s(
            duration, L"%02d:%02d:%02d",
            snapshot.remainingSeconds / 3600,
            (snapshot.remainingSeconds % 3600) / 60,
            snapshot.remainingSeconds % 60);
        countdown = duration;
    }

    DrawTextInRect(dc, status, ToRect(kStatusText, scale, dpi),
                   model.fonts->label, kTextSecondary);
    DrawTextInRect(dc, countdown, ToRect(kCountdownText, scale, dpi),
                   model.fonts->countdown, kTextPrimary);

    FillRounded(dc, ToRect(kProgressTrackRect, scale, dpi),
                ScaleValue(2, scale, dpi), kProgressTrack);
    const int progressRight =
        kProgressTrackRect.left +
        static_cast<int>((kProgressTrackRect.right - kProgressTrackRect.left) *
                         snapshot.progress);
    if (progressRight > kProgressTrackRect.left) {
        const LogicalRect filled{
            kProgressTrackRect.left, kProgressTrackRect.top,
            progressRight, kProgressTrackRect.bottom};
        FillRounded(dc, ToRect(filled, scale, dpi), ScaleValue(2, scale, dpi), kAccent);
    }

    wchar_t progress[48] = {};
    swprintf_s(progress, L"今日进度 %.0f%%", snapshot.progress * 100.0);
    DrawTextInRect(
        dc, progress, ToRect(kProgressText, scale, dpi), model.fonts->tiny,
        kTextSecondary, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DrawTextInRect(dc, L"今天已经赚了", ToRect(kEarnedLabel, scale, dpi),
                   model.fonts->label, kTextSecondary);
    wchar_t earned[64] = {};
    swprintf_s(earned, L"¥%.2f", snapshot.earned);
    DrawTextInRect(dc, earned, ToRect(kEarnedAmount, scale, dpi),
                   model.fonts->amount, kEarned);
    wchar_t salary[64] = {};
    swprintf_s(salary, L"日薪 ¥%.2f", model.settings.dailySalary);
    DrawTextInRect(
        dc, salary, ToRect(kSalaryText, scale, dpi), model.fonts->tiny,
        kTextSecondary, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    FillRounded(
        dc, ToRect(kSettingsButton, scale, dpi), ScaleValue(6, scale, dpi),
        model.hot == HotElement::Settings ? kButtonHover : kButtonBackground);
    DrawTextInRect(dc, L"设置", ToRect(kSettingsLabel, scale, dpi),
                   model.fonts->button, kTextPrimary);
    DrawChevron(dc, model);

    if (!model.expanded) {
        return;
    }

    FillRounded(dc, ToRect(kPanel, scale, dpi), ScaleValue(6, scale, dpi),
                kPanelBackground);

    DrawTextInRect(dc, L"上班时间", ToRect(kStartLabel, scale, dpi),
                   model.fonts->label, kTextPrimary);
    DrawTextInRect(dc, L"下班时间", ToRect(kEndLabel, scale, dpi),
                   model.fonts->label, kTextPrimary);
    DrawTextInRect(dc, L"日薪（元）", ToRect(kSalaryLabel, scale, dpi),
                   model.fonts->label, kTextPrimary);

    DrawField(dc, model, 0);
    DrawField(dc, model, 1);
    DrawField(dc, model, 2);

    wchar_t opacityLabel[32] = {};
    swprintf_s(opacityLabel, L"不透明度 %d%%", model.settings.opacityPercent);
    DrawTextInRect(dc, opacityLabel, ToRect(kOpacityLabel, scale, dpi),
                   model.fonts->label, kTextPrimary);
    DrawOpacitySlider(dc, model);

    DrawTextInRect(dc, L"开机自启", ToRect(kAutostartLabel, scale, dpi),
                   model.fonts->label, kTextPrimary);
    DrawAutostartToggle(dc, model);

    FillRounded(
        dc, ToRect(kSaveButton, scale, dpi), ScaleValue(6, scale, dpi),
        model.hot == HotElement::Save ? kAccentHover : kAccent);
    DrawTextInRect(
        dc, L"保存设置", ToRect(kSaveButton, scale, dpi), model.fonts->button,
        kOnAccent, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DrawTextInRect(
        dc, model.footerText, ToRect(kFooterText, scale, dpi), model.fonts->tiny,
        model.footerIsError ? kDanger : kTextMuted,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

}  // namespace offwork
