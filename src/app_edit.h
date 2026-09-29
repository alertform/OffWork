#pragma once

// Text fields, clipboard, and the actions the settings panel triggers.

#include "app_state.h"

#include <windows.h>

#include <cstddef>
#include <string>

namespace offwork {

FieldFilter FilterFor(int fieldIndex);
std::size_t MaxLengthFor(int fieldIndex);
std::wstring FormatTimeValue(int minutes);
std::wstring FormatSalaryValue(double salary);
std::wstring FormatDurationHours(int durationMinutes);
void PopulateFields();
bool ParseTimeText(const std::wstring& text, int& minutes);
bool ReadFieldSettings(Settings& settings);

std::wstring ReadClipboardText(HWND owner);
void WriteClipboardText(HWND owner, const std::wstring& text);

void ApplySave();
void ToggleAutostart();
void TogglePinned();
void SetEndInputMode(EndInputMode mode);

void FocusNextField(int direction);
bool HandleFieldKey(WPARAM key);

}  // namespace offwork
