#include "autostart.h"

#include <windows.h>

#include <vector>

namespace offwork {
namespace {

std::wstring FormatSystemError(LSTATUS status) {
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(status), 0,
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message;
    if (length != 0 && buffer != nullptr) {
        message.assign(buffer, length);
        while (!message.empty() &&
               (message.back() == L'\r' || message.back() == L'\n')) {
            message.pop_back();
        }
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    if (message.empty()) {
        message = L"错误代码 " + std::to_wstring(status);
    }
    return message;
}

}  // namespace

std::wstring QuoteExecutablePath(const std::wstring& executablePath) {
    return L"\"" + executablePath + L"\"";
}

std::wstring UnquoteExecutablePath(const std::wstring& runValue) {
    if (runValue.size() >= 2 && runValue.front() == L'"' && runValue.back() == L'"') {
        return runValue.substr(1, runValue.size() - 2);
    }
    return runValue;
}

std::wstring CurrentExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD written = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0) {
            return {};
        }
        if (written < buffer.size()) {
            return std::wstring(buffer.data(), written);
        }
        if (buffer.size() >= 32768) {
            return {};
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring ReadAutostartCommand() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = RegQueryValueExW(key, kRunValueName, nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        RegCloseKey(key);
        return {};
    }

    std::wstring value(bytes / sizeof(wchar_t) + 1, L'\0');
    bytes = static_cast<DWORD>((value.size()) * sizeof(wchar_t));
    status = RegQueryValueExW(
        key, kRunValueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(value.data()), &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return {};
    }

    value.resize(bytes / sizeof(wchar_t));
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

bool IsAutostartEnabled() {
    return !ReadAutostartCommand().empty();
}

AutostartResult SetAutostartEnabled(bool enabled) {
    AutostartResult result;

    if (!enabled) {
        HKEY key = nullptr;
        LSTATUS status =
            RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key);
        if (status != ERROR_SUCCESS) {
            result.message = L"无法打开启动项: " + FormatSystemError(status);
            return result;
        }
        status = RegDeleteValueW(key, kRunValueName);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
            result.message = L"无法关闭开机自启: " + FormatSystemError(status);
            return result;
        }
        result.ok = true;
        return result;
    }

    const std::wstring executable = CurrentExecutablePath();
    if (executable.empty()) {
        result.message = L"无法读取程序路径, 开机自启未改动";
        return result;
    }

    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        result.message = L"无法打开启动项: " + FormatSystemError(status);
        return result;
    }

    const std::wstring command = QuoteExecutablePath(executable);
    status = RegSetValueExW(
        key, kRunValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        result.message = L"无法开启开机自启: " + FormatSystemError(status);
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace offwork
