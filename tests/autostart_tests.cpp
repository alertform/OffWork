// Only the pure helpers are unit tested here. The registry round-trip is
// deliberately NOT exercised: this test runs in CI and on developer machines,
// and writing the real HKCU Run value would change the machine it runs on. The
// live behaviour is verified on the desktop instead, as the task requires.

#include "autostart.h"

#include "check.h"

#include <iostream>

namespace {

using offwork::QuoteExecutablePath;
using offwork::UnquoteExecutablePath;

void QuotingWrapsThePathSoSpacesSurvive() {
    CHECK(QuoteExecutablePath(L"C:\\Program Files\\OffWork\\OffWork.exe") ==
          L"\"C:\\Program Files\\OffWork\\OffWork.exe\"");
    CHECK(QuoteExecutablePath(L"D:\\OffWork.exe") == L"\"D:\\OffWork.exe\"");
    CHECK(QuoteExecutablePath(L"") == L"\"\"");
}

void UnquotingIsTheInverseOfQuoting() {
    const wchar_t* paths[] = {
        L"C:\\Program Files\\OffWork\\OffWork.exe",
        L"D:\\OffWork.exe",
        L"C:\\Users\\Someone\\AppData\\Local\\Programs\\OffWork\\OffWork.exe",
    };
    for (const wchar_t* path : paths) {
        CHECK(UnquoteExecutablePath(QuoteExecutablePath(path)) == path);
    }
}

void UnquotingLeavesAnUnquotedValueAlone() {
    CHECK(UnquoteExecutablePath(L"D:\\OffWork.exe") == L"D:\\OffWork.exe");
    CHECK(UnquoteExecutablePath(L"") == L"");
    CHECK(UnquoteExecutablePath(L"\"") == L"\"");
}

void RegistryLocationMatchesTheInstallerScript() {
    // The Inno Setup script writes the same key and value name; if either side
    // is edited alone the app and the installer stop agreeing.
    CHECK(std::wstring(offwork::kRunKeyPath) ==
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    CHECK(std::wstring(offwork::kRunValueName) == L"OffWork");
}

}  // namespace

int main() {
    QuotingWrapsThePathSoSpacesSurvive();
    UnquotingIsTheInverseOfQuoting();
    UnquotingLeavesAnUnquotedValueAlone();
    RegistryLocationMatchesTheInstallerScript();

    std::cout << "All OffWork autostart tests passed.\n";
    return 0;
}
