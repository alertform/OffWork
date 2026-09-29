#pragma once

// State changes that end in a frame being published.

#include "app_state.h"

#include <windows.h>

#include <string>

namespace offwork {

void EnsureFonts(int scalePercent);
RenderModel BuildModel();

// The single place where anything reaches the screen: size, position and pixels
// always leave together.
void PresentAt(POINT topLeft, int scalePercent);
void Present();

void SetFooter(std::wstring text, bool isError);
void ResetFooter();
void RefreshAutostart();
void SetFocusedField(int index);
void SetExpanded(bool expanded);
void EnsureTopmost();

}  // namespace offwork
