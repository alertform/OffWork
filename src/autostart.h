#pragma once

// Start-at-login support.
//
// The single source of truth is the per-user Run key:
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Run  ->  value "OffWork"
// Nothing is mirrored into settings.ini, so the app and the installer cannot
// drift apart, and a change made outside the app is picked up on the next read.

#include <string>

namespace offwork {

// Registry location, shared with the Inno Setup script.
inline constexpr wchar_t kRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t kRunValueName[] = L"OffWork";

// Pure: the exact string stored in the Run value. Quoted so that a path
// containing spaces is launched as one argument.
std::wstring QuoteExecutablePath(const std::wstring& executablePath);

// Pure: the path a quoted Run value points at, for comparing against the
// running executable. Returns the input unchanged when it is not quoted.
std::wstring UnquoteExecutablePath(const std::wstring& runValue);

struct AutostartResult {
    bool ok = false;
    std::wstring message;  // user-facing, non-empty when ok is false
};

// Full path of the running executable, or an empty string if it cannot be read.
std::wstring CurrentExecutablePath();

// True when the Run value exists. The value may point at another copy of
// OffWork (installed vs portable); enabling always rewrites it to this one.
bool IsAutostartEnabled();

// Reads the raw Run value, empty when absent.
std::wstring ReadAutostartCommand();

AutostartResult SetAutostartEnabled(bool enabled);

}  // namespace offwork
