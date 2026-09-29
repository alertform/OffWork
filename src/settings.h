#pragma once

// Persisted settings. Start-at-login is deliberately absent: its truth lives
// in the registry (see autostart.h), never duplicated here.

#include <string>

namespace offwork {

struct Settings {
    int startMinutes = 9 * 60;
    int endMinutes = 18 * 60;
    double dailySalary = 500.0;
    int uiScalePercent = 100;
    int opacityPercent = 100;
};

// Pure: clamps every field into range and falls back to the defaults for a
// work day that cannot exist. Returns a new value; the input is untouched.
Settings NormalizeSettings(const Settings& settings);

std::wstring SettingsFilePath();
Settings LoadSettings(const std::wstring& path);
void SaveSettings(const std::wstring& path, const Settings& settings);

}  // namespace offwork
