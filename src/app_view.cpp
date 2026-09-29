#include "app_view.h"

#include "app_edit.h"
#include "autostart.h"

#include <algorithm>

namespace offwork {

AppState g;

void EnsureFonts(int scalePercent) {
    if (g.fontScale == scalePercent && g.fonts.label != nullptr) {
        return;
    }
    DestroyFonts(g.fonts);
    g.fonts = CreateFonts(scalePercent, g.dpi);
    g.fontScale = scalePercent;
}

RenderModel BuildModel() {
    RenderModel model;
    model.settings = g.settings;
    model.dpi = g.dpi;
    model.expanded = g.expanded;
    model.pinned = g.pinned;
    model.autostartEnabled = g.autostartEnabled;
    model.editEndInputMode = g.editEndInputMode;
    model.hot = g.hot;
    model.icon = g.icon;
    model.fonts = &g.fonts;
    model.startField = g.fields[0];
    model.endField = g.fields[1];
    model.salaryField = g.fields[2];
    model.focusedField = g.expanded ? g.focusedField : -1;
    model.caretVisible = g.caretVisible;
    model.footerText = g.footerText;
    model.footerIsError = g.footerIsError;
    GetLocalTime(&model.now);
    return model;
}

// The single place where anything reaches the screen. Size, position and
// pixels always leave together.
void PresentAt(POINT topLeft, int scalePercent) {
    if (g.window == nullptr) {
        return;
    }
    g.settings.uiScalePercent =
        std::clamp(scalePercent, kMinScalePercent, kMaxScalePercent);
    EnsureFonts(g.settings.uiScalePercent);

    const SIZE size = WindowSizeFor(g.settings.uiScalePercent, g.dpi, g.expanded);
    if (!g.surface.EnsureSize(size.cx, size.cy)) {
        return;
    }

    const RenderModel model = BuildModel();
    RenderFrame(g.surface.dc(), model);
    g.surface.ApplyRoundedAlpha(
        ScaleValue(kCornerRadius, g.settings.uiScalePercent, g.dpi));

    const auto alpha = static_cast<BYTE>(
        MulDiv(std::clamp(g.settings.opacityPercent, kMinOpacityPercent, 100), 255, 100));
    g.surface.Present(g.window, topLeft, alpha);
}

void Present() {
    RECT rect = {};
    if (g.window == nullptr || GetWindowRect(g.window, &rect) == 0) {
        return;
    }
    PresentAt({rect.left, rect.top}, g.settings.uiScalePercent);
}

void SetFooter(std::wstring text, bool isError) {
    g.footerText = std::move(text);
    g.footerIsError = isError;
}

void ResetFooter() {
    SetFooter(kDefaultFooter, false);
}

void RefreshAutostart() {
    g.autostartEnabled = IsAutostartEnabled();
}

void SetFocusedField(int index) {
    if (g.focusedField == index) {
        return;
    }
    g.focusedField = index;
    g.caretVisible = index >= 0;
    if (index >= 0) {
        SetTimer(g.window, kCaretTimer, kCaretBlinkMs, nullptr);
    } else {
        KillTimer(g.window, kCaretTimer);
        g.caretVisible = false;
    }
}

void SetExpanded(bool expanded) {
    if (g.expanded == expanded) {
        return;
    }
    g.expanded = expanded;
    if (expanded) {
        PopulateFields();
        RefreshAutostart();
        ResetFooter();
        SetFocusedField(-1);
    } else {
        SetFocusedField(-1);
        ResetFooter();
    }
    // Collapsing and expanding are just another atomic size change.
    Present();
}

void EnsureTopmost() {
    if (g.pinned && g.window != nullptr) {
        SetWindowPos(
            g.window, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

}  // namespace offwork
