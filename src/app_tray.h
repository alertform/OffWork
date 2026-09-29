#pragma once

#include <windows.h>

namespace offwork {

void InitializeTray();
bool HandleTrayMessage(UINT message, WPARAM wParam, LPARAM lParam);

// Hides the widget only when a working tray icon can bring it back. Returns
// false when the caller should close the process instead.
bool HideWidgetToTray();
void RemoveTray();

}  // namespace offwork
