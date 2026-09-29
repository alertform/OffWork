#pragma once

#include <windows.h>

namespace offwork {

// Registers the window class, creates the widget and runs the message loop.
// Returns the value to hand back from wWinMain.
int RunApp(HINSTANCE instance, int showCommand);

}  // namespace offwork
