#pragma once

// Editing model for the self-drawn text fields. Deliberately free of any
// Windows or HWND dependency so it can be unit tested on its own: the window
// only translates messages into these calls and draws the resulting state.
//
// Every operation returns a new FieldState instead of mutating its argument.

#include <cstddef>
#include <string>
#include <string_view>

namespace offwork {

enum class FieldFilter {
    Time,     // digits plus a single colon
    Decimal,  // digits plus a single dot
};

struct FieldState {
    std::wstring text;
    std::size_t caret = 0;   // insertion point, 0..text.size()
    std::size_t anchor = 0;  // other end of the selection; == caret when empty
};

FieldState MakeField(std::wstring text);

std::size_t SelectionBegin(const FieldState& state);
std::size_t SelectionEnd(const FieldState& state);
bool HasSelection(const FieldState& state);
std::wstring SelectedText(const FieldState& state);

bool IsCharAllowed(wchar_t character, FieldFilter filter);

FieldState MoveCaretTo(const FieldState& state, std::size_t position, bool extend);
FieldState MoveCaret(const FieldState& state, int delta, bool extend);
FieldState MoveToStart(const FieldState& state, bool extend);
FieldState MoveToEnd(const FieldState& state, bool extend);
FieldState SelectAll(const FieldState& state);
FieldState ClearSelection(const FieldState& state);

FieldState DeleteSelection(const FieldState& state);
FieldState DeleteBackward(const FieldState& state);
FieldState DeleteForward(const FieldState& state);

// Inserts the allowed characters of the input at the caret, replacing any
// selection. Characters rejected by the filter, or that would exceed
// maxLength, are dropped; the rest are still inserted.
FieldState InsertText(
    const FieldState& state,
    std::wstring_view input,
    FieldFilter filter,
    std::size_t maxLength);

}  // namespace offwork
