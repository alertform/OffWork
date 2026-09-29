#include "settings.h"

#include "theme.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <iterator>

namespace offwork {
namespace {

constexpr int kMinutesPerDay = 24 * 60;
constexpr double kMaxDailySalary = 1000000.0;

}  // namespace

Settings NormalizeSettings(const Settings& settings) {
    Settings normalized = settings;

    if (settings.endInputMode != EndInputMode::EndTime &&
        settings.endInputMode != EndInputMode::WorkDuration) {
        normalized.endInputMode = EndInputMode::EndTime;
    }

    normalized.uiScalePercent =
        std::clamp(settings.uiScalePercent, kMinScalePercent, kMaxScalePercent);
    normalized.opacityPercent =
        std::clamp(settings.opacityPercent, kMinOpacityPercent, 100);

    if (!std::isfinite(settings.dailySalary) || settings.dailySalary < 0.0) {
        normalized.dailySalary = 0.0;
    } else {
        normalized.dailySalary = std::min(settings.dailySalary, kMaxDailySalary);
    }

    const bool startValid =
        settings.startMinutes >= 0 && settings.startMinutes < kMinutesPerDay;
    const bool endValid =
        settings.endMinutes >= 0 && settings.endMinutes < kMinutesPerDay;
    if (!startValid || !endValid || settings.startMinutes == settings.endMinutes) {
        normalized.startMinutes = 9 * 60;
        normalized.endMinutes = 18 * 60;
    }

    return normalized;
}

std::wstring SettingsFilePath() {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData))) {
        return L"OffWork.ini";
    }

    std::filesystem::path directory(localAppData);
    CoTaskMemFree(localAppData);
    directory /= L"OffWork";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return (directory / L"settings.ini").wstring();
}

Settings LoadSettings(const std::wstring& path) {
    Settings settings;
    settings.startMinutes = GetPrivateProfileIntW(
        L"work", L"startMinutes", settings.startMinutes, path.c_str());
    settings.endMinutes = GetPrivateProfileIntW(
        L"work", L"endMinutes", settings.endMinutes, path.c_str());
    settings.uiScalePercent = static_cast<int>(GetPrivateProfileIntW(
        L"window", L"scalePercent", settings.uiScalePercent, path.c_str()));
    settings.opacityPercent = static_cast<int>(GetPrivateProfileIntW(
        L"window", L"opacityPercent", settings.opacityPercent, path.c_str()));

    wchar_t inputMode[32] = {};
    GetPrivateProfileStringW(
        L"work", L"endInputMode", L"endTime", inputMode,
        static_cast<DWORD>(std::size(inputMode)), path.c_str());
    settings.endInputMode = _wcsicmp(inputMode, L"duration") == 0
        ? EndInputMode::WorkDuration
        : EndInputMode::EndTime;

    wchar_t salary[64] = {};
    GetPrivateProfileStringW(
        L"work", L"dailySalary", L"500", salary,
        static_cast<DWORD>(std::size(salary)), path.c_str());
    wchar_t* end = nullptr;
    const double parsedSalary = std::wcstod(salary, &end);
    if (end != salary && std::isfinite(parsedSalary) && parsedSalary >= 0.0) {
        settings.dailySalary = parsedSalary;
    }

    return NormalizeSettings(settings);
}

void SaveSettings(const std::wstring& path, const Settings& settings) {
    const Settings normalized = NormalizeSettings(settings);
    wchar_t value[64] = {};

    swprintf_s(value, L"%d", normalized.startMinutes);
    WritePrivateProfileStringW(L"work", L"startMinutes", value, path.c_str());
    swprintf_s(value, L"%d", normalized.endMinutes);
    WritePrivateProfileStringW(L"work", L"endMinutes", value, path.c_str());
    WritePrivateProfileStringW(
        L"work", L"endInputMode",
        normalized.endInputMode == EndInputMode::WorkDuration
            ? L"duration"
            : L"endTime",
        path.c_str());
    swprintf_s(value, L"%.2f", normalized.dailySalary);
    WritePrivateProfileStringW(L"work", L"dailySalary", value, path.c_str());
    swprintf_s(value, L"%d", normalized.uiScalePercent);
    WritePrivateProfileStringW(L"window", L"scalePercent", value, path.c_str());
    swprintf_s(value, L"%d", normalized.opacityPercent);
    WritePrivateProfileStringW(L"window", L"opacityPercent", value, path.c_str());
}

}  // namespace offwork
