#pragma once

// Drawing. RenderFrame is a pure function of RenderModel, which is what makes
// the coherent resize possible: during a drag the window renders a frame for
// the size it is *about to* have, and that frame and the new geometry are then
// published together by LayeredSurface::Present.

#include "settings.h"
#include "text_field.h"
#include "theme.h"

#include <windows.h>

#include <cstddef>
#include <string>

namespace offwork {

struct FontSet {
    HFONT title = nullptr;
    HFONT tiny = nullptr;
    HFONT label = nullptr;
    HFONT countdown = nullptr;
    HFONT amount = nullptr;
    HFONT button = nullptr;
};

FontSet CreateFonts(int scalePercent, UINT dpi);
void DestroyFonts(FontSet& fonts);

struct RenderModel {
    Settings settings;
    UINT dpi = 96;
    bool expanded = false;
    bool pinned = true;
    bool autostartEnabled = false;
    EndInputMode editEndInputMode = EndInputMode::EndTime;
    HotElement hot = HotElement::None;
    HICON icon = nullptr;
    const FontSet* fonts = nullptr;

    FieldState startField;
    FieldState endField;
    FieldState salaryField;
    int focusedField = -1;  // 0 start, 1 end, 2 salary, -1 none
    bool caretVisible = false;

    std::wstring footerText;
    bool footerIsError = false;

    SYSTEMTIME now = {};
};

// -- geometry ---------------------------------------------------------------

int ScaleValue(int logical, int scalePercent, UINT dpi);
int UnscaleValue(int device, int scalePercent, UINT dpi);
RECT ScaleRect(const LogicalRect& rect, int scalePercent, UINT dpi);
SIZE WindowSizeFor(int scalePercent, UINT dpi, bool expanded);

// Scale implied by a window width, clamped to the supported range.
int ScaleFromWidth(int deviceWidth, UINT dpi);

// -- drawing ----------------------------------------------------------------

void RenderFrame(HDC dc, const RenderModel& model);

// -- hit testing ------------------------------------------------------------

HotElement HitTest(POINT clientPoint, const RenderModel& model);
int OpacityFromX(int clientX, int scalePercent, UINT dpi);

// Index of the field owning a hot element, or -1.
int FieldIndexOf(HotElement element);
const LogicalRect& FieldBox(int fieldIndex);

// Caret geometry for the self-drawn fields. Both need a DC with the field font
// selected, which RenderFrame and the window both have.
std::size_t CaretIndexFromX(HDC dc, const RenderModel& model, int fieldIndex, int clientX);

}  // namespace offwork
