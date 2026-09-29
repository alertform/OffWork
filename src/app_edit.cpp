#include "app_edit.h"

#include "app_view.h"
#include "autostart.h"
#include "calculator.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace offwork {

// -- small helpers ----------------------------------------------------------

FieldFilter FilterFor(int fieldIndex) {
    return fieldIndex == 2 ||
            (fieldIndex == 1 && g.editEndInputMode == EndInputMode::WorkDuration)
        ? FieldFilter::Decimal
        : FieldFilter::Time;
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

std::wstring FormatDurationHours(int durationMinutes) {
    wchar_t text[32] = {};
    swprintf_s(text, L"%.2f", static_cast<double>(durationMinutes) / 60.0);
    std::wstring value(text);
    while (value.size() > 1 && value.back() == L'0') {
        value.pop_back();
    }
    if (!value.empty() && value.back() == L'.') {
        value.pop_back();
    }
    return value;
}

bool ParseDurationText(const std::wstring& text, int& durationMinutes) {
    if (text.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const double hours = std::wcstod(text.c_str(), &end);
    if (end == nullptr || *end != L'\0' || !std::isfinite(hours) ||
        hours <= 0.0 || hours >= 24.0) {
        return false;
    }
    try {
        durationMinutes = DurationMinutesFromHours(hours);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

void PopulateFields() {
    g.editEndInputMode = g.settings.endInputMode;
    g.fields[0] = MakeField(FormatTimeValue(g.settings.startMinutes));
    g.fields[1] = MakeField(
        g.editEndInputMode == EndInputMode::WorkDuration
            ? FormatDurationHours(
                  WorkDurationMinutes(
                      g.settings.startMinutes, g.settings.endMinutes))
            : FormatTimeValue(g.settings.endMinutes));
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
    if (!ParseTimeText(g.fields[0].text, settings.startMinutes)) {
        return false;
    }

    settings.endInputMode = g.editEndInputMode;
    if (g.editEndInputMode == EndInputMode::WorkDuration) {
        int durationMinutes = 0;
        if (!ParseDurationText(g.fields[1].text, durationMinutes)) {
            return false;
        }
        settings.endMinutes =
            EndMinutesFromDuration(settings.startMinutes, durationMinutes);
    } else if (!ParseTimeText(g.fields[1].text, settings.endMinutes) ||
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

void SetEndInputMode(EndInputMode mode) {
    if (g.editEndInputMode == mode) {
        return;
    }

    int startMinutes = g.settings.startMinutes;
    ParseTimeText(g.fields[0].text, startMinutes);
    int durationMinutes = WorkDurationMinutes(
        g.settings.startMinutes, g.settings.endMinutes);

    if (mode == EndInputMode::WorkDuration) {
        int endMinutes = g.settings.endMinutes;
        if (ParseTimeText(g.fields[1].text, endMinutes) &&
            endMinutes != startMinutes) {
            durationMinutes = WorkDurationMinutes(startMinutes, endMinutes);
        }
        g.fields[1] = MakeField(FormatDurationHours(durationMinutes));
    } else {
        ParseDurationText(g.fields[1].text, durationMinutes);
        g.fields[1] = MakeField(FormatTimeValue(
            EndMinutesFromDuration(startMinutes, durationMinutes)));
    }

    g.editEndInputMode = mode;
    if (g.focusedField == 1) {
        g.fields[1] = SelectAll(g.fields[1]);
    }
    ResetFooter();
    Present();
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

}  // namespace offwork
