#include "text_field.h"

#include <algorithm>

namespace offwork {
namespace {

wchar_t SeparatorFor(FieldFilter filter) {
    return filter == FieldFilter::Time ? L':' : L'.';
}

std::size_t ClampPosition(const FieldState& state, std::size_t position) {
    return std::min(position, state.text.size());
}

FieldState Collapsed(std::wstring text, std::size_t caret) {
    FieldState state;
    state.text = std::move(text);
    state.caret = caret;
    state.anchor = caret;
    return state;
}

}  // namespace

FieldState MakeField(std::wstring text) {
    const std::size_t end = text.size();
    return Collapsed(std::move(text), end);
}

std::size_t SelectionBegin(const FieldState& state) {
    return std::min(state.caret, state.anchor);
}

std::size_t SelectionEnd(const FieldState& state) {
    return std::max(state.caret, state.anchor);
}

bool HasSelection(const FieldState& state) {
    return state.caret != state.anchor;
}

std::wstring SelectedText(const FieldState& state) {
    if (!HasSelection(state)) {
        return {};
    }
    const std::size_t begin = SelectionBegin(state);
    return state.text.substr(begin, SelectionEnd(state) - begin);
}

bool IsCharAllowed(wchar_t character, FieldFilter filter) {
    if (character >= L'0' && character <= L'9') {
        return true;
    }
    return character == SeparatorFor(filter);
}

FieldState MoveCaretTo(const FieldState& state, std::size_t position, bool extend) {
    FieldState next = state;
    next.caret = ClampPosition(state, position);
    if (!extend) {
        next.anchor = next.caret;
    }
    return next;
}

FieldState MoveCaret(const FieldState& state, int delta, bool extend) {
    // Collapsing a selection with an unmodified arrow key jumps to the edge the
    // key points at, without also consuming the keystroke as a move.
    if (!extend && HasSelection(state)) {
        const std::size_t edge = delta < 0 ? SelectionBegin(state) : SelectionEnd(state);
        return MoveCaretTo(state, edge, false);
    }

    std::size_t target = state.caret;
    if (delta < 0) {
        const auto magnitude = static_cast<std::size_t>(-static_cast<long long>(delta));
        target = magnitude > state.caret ? 0 : state.caret - magnitude;
    } else {
        target = state.caret + static_cast<std::size_t>(delta);
    }
    return MoveCaretTo(state, target, extend);
}

FieldState MoveToStart(const FieldState& state, bool extend) {
    return MoveCaretTo(state, 0, extend);
}

FieldState MoveToEnd(const FieldState& state, bool extend) {
    return MoveCaretTo(state, state.text.size(), extend);
}

FieldState SelectAll(const FieldState& state) {
    FieldState next = state;
    next.anchor = 0;
    next.caret = state.text.size();
    return next;
}

FieldState ClearSelection(const FieldState& state) {
    FieldState next = state;
    next.anchor = next.caret;
    return next;
}

FieldState DeleteSelection(const FieldState& state) {
    if (!HasSelection(state)) {
        return state;
    }
    const std::size_t begin = SelectionBegin(state);
    const std::size_t end = SelectionEnd(state);
    std::wstring text = state.text;
    text.erase(begin, end - begin);
    return Collapsed(std::move(text), begin);
}

FieldState DeleteBackward(const FieldState& state) {
    if (HasSelection(state)) {
        return DeleteSelection(state);
    }
    if (state.caret == 0) {
        return state;
    }
    std::wstring text = state.text;
    text.erase(state.caret - 1, 1);
    return Collapsed(std::move(text), state.caret - 1);
}

FieldState DeleteForward(const FieldState& state) {
    if (HasSelection(state)) {
        return DeleteSelection(state);
    }
    if (state.caret >= state.text.size()) {
        return state;
    }
    std::wstring text = state.text;
    text.erase(state.caret, 1);
    return Collapsed(std::move(text), state.caret);
}

FieldState InsertText(
    const FieldState& state,
    std::wstring_view input,
    FieldFilter filter,
    std::size_t maxLength) {
    FieldState next = DeleteSelection(state);
    const wchar_t separator = SeparatorFor(filter);

    for (const wchar_t character : input) {
        if (!IsCharAllowed(character, filter)) {
            continue;
        }
        if (character == separator &&
            next.text.find(separator) != std::wstring::npos) {
            continue;
        }
        if (next.text.size() >= maxLength) {
            continue;
        }
        next.text.insert(next.caret, 1, character);
        ++next.caret;
    }

    next.anchor = next.caret;
    return next;
}

}  // namespace offwork
