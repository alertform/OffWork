#include "text_field.h"

#include "check.h"

#include <iostream>

namespace {

using offwork::ClearSelection;
using offwork::DeleteBackward;
using offwork::DeleteForward;
using offwork::DeleteSelection;
using offwork::FieldFilter;
using offwork::FieldState;
using offwork::HasSelection;
using offwork::InsertText;
using offwork::IsCharAllowed;
using offwork::MakeField;
using offwork::MoveCaret;
using offwork::MoveCaretTo;
using offwork::MoveToEnd;
using offwork::MoveToStart;
using offwork::SelectAll;
using offwork::SelectedText;
using offwork::SelectionBegin;
using offwork::SelectionEnd;

constexpr std::size_t kTimeMax = 5;
constexpr std::size_t kSalaryMax = 16;

void MakeFieldStartsCollapsedAtEnd() {
    const FieldState field = MakeField(L"09:00");
    CHECK(field.text == L"09:00");
    CHECK(field.caret == 5);
    CHECK(field.anchor == 5);
    CHECK(!HasSelection(field));
    CHECK(SelectedText(field).empty());
}

void FilterAcceptsOnlyItsOwnCharacters() {
    CHECK(IsCharAllowed(L'0', FieldFilter::Time));
    CHECK(IsCharAllowed(L'9', FieldFilter::Time));
    CHECK(IsCharAllowed(L':', FieldFilter::Time));
    CHECK(!IsCharAllowed(L'.', FieldFilter::Time));
    CHECK(!IsCharAllowed(L'a', FieldFilter::Time));
    CHECK(!IsCharAllowed(L'下', FieldFilter::Time));

    CHECK(IsCharAllowed(L'7', FieldFilter::Decimal));
    CHECK(IsCharAllowed(L'.', FieldFilter::Decimal));
    CHECK(!IsCharAllowed(L':', FieldFilter::Decimal));
    CHECK(!IsCharAllowed(L'-', FieldFilter::Decimal));
}

void InsertPutsTextAtTheCaret() {
    FieldState field = MakeField(L"0900");
    field = MoveCaretTo(field, 2, false);
    field = InsertText(field, L":", FieldFilter::Time, kTimeMax);
    CHECK(field.text == L"09:00");
    CHECK(field.caret == 3);
    CHECK(!HasSelection(field));
}

void InsertDropsRejectedCharactersButKeepsTheRest() {
    FieldState field = MakeField(L"");
    field = InsertText(field, L"1a2b:3c", FieldFilter::Time, kTimeMax);
    CHECK(field.text == L"12:3");
    CHECK(field.caret == 4);
}

void InsertRefusesASecondSeparator() {
    FieldState field = MakeField(L"09:00");
    field = MoveToEnd(field, false);
    field = InsertText(field, L":", FieldFilter::Time, kTimeMax);
    CHECK(field.text == L"09:00");

    FieldState salary = MakeField(L"12.5");
    salary = MoveToEnd(salary, false);
    salary = InsertText(salary, L".", FieldFilter::Decimal, kSalaryMax);
    CHECK(salary.text == L"12.5");
}

void InsertStopsAtMaxLength() {
    FieldState field = MakeField(L"09:00");
    field = MoveToEnd(field, false);
    field = InsertText(field, L"7", FieldFilter::Time, kTimeMax);
    CHECK(field.text == L"09:00");
    CHECK(field.caret == 5);
}

void InsertReplacesTheSelection() {
    FieldState field = MakeField(L"09:00");
    field = SelectAll(field);
    CHECK(HasSelection(field));
    field = InsertText(field, L"18:30", FieldFilter::Time, kTimeMax);
    CHECK(field.text == L"18:30");
    CHECK(field.caret == 5);
    CHECK(!HasSelection(field));
}

void SelectionSpansTheSmallerToLargerEnd() {
    FieldState field = MakeField(L"18:30");
    field = MoveCaretTo(field, 4, false);
    field = MoveCaretTo(field, 1, true);
    CHECK(SelectionBegin(field) == 1);
    CHECK(SelectionEnd(field) == 4);
    CHECK(SelectedText(field) == L"8:3");
    CHECK(HasSelection(field));
}

void SelectAllThenTypingKeepsTheFieldValid() {
    FieldState salary = MakeField(L"500");
    salary = SelectAll(salary);
    CHECK(SelectedText(salary) == L"500");
    salary = InsertText(salary, L"1234.56", FieldFilter::Decimal, kSalaryMax);
    CHECK(salary.text == L"1234.56");
}

void DeleteBackwardRemovesOneCharacterOrTheSelection() {
    FieldState field = MakeField(L"09:00");
    field = DeleteBackward(field);
    CHECK(field.text == L"09:0");
    CHECK(field.caret == 4);

    FieldState selected = MakeField(L"09:00");
    selected = SelectAll(selected);
    selected = DeleteBackward(selected);
    CHECK(selected.text.empty());
    CHECK(selected.caret == 0);
    CHECK(!HasSelection(selected));
}

void DeleteBackwardAtStartIsANoOp() {
    FieldState field = MakeField(L"09:00");
    field = MoveToStart(field, false);
    field = DeleteBackward(field);
    CHECK(field.text == L"09:00");
    CHECK(field.caret == 0);
}

void DeleteForwardRemovesTheCharacterAfterTheCaret() {
    FieldState field = MakeField(L"09:00");
    field = MoveToStart(field, false);
    field = DeleteForward(field);
    CHECK(field.text == L"9:00");
    CHECK(field.caret == 0);

    FieldState atEnd = MakeField(L"09:00");
    atEnd = DeleteForward(atEnd);
    CHECK(atEnd.text == L"09:00");
}

void DeleteSelectionCollapsesTheCaretToTheSelectionStart() {
    FieldState field = MakeField(L"09:00");
    field = MoveCaretTo(field, 0, false);
    field = MoveCaretTo(field, 3, true);
    field = DeleteSelection(field);
    CHECK(field.text == L"00");
    CHECK(field.caret == 0);
    CHECK(!HasSelection(field));
}

void MoveCaretClampsToTheText() {
    FieldState field = MakeField(L"500");
    field = MoveCaret(field, 10, false);
    CHECK(field.caret == 3);
    field = MoveCaret(field, -10, false);
    CHECK(field.caret == 0);
    field = MoveCaretTo(field, 99, false);
    CHECK(field.caret == 3);
}

void MoveCaretWithoutExtendCollapsesTheSelection() {
    FieldState field = MakeField(L"09:00");
    field = SelectAll(field);
    field = MoveCaret(field, -1, false);
    CHECK(!HasSelection(field));
}

void ClearSelectionKeepsTheCaret() {
    FieldState field = MakeField(L"09:00");
    field = MoveCaretTo(field, 1, false);
    field = MoveCaretTo(field, 4, true);
    field = ClearSelection(field);
    CHECK(field.caret == 4);
    CHECK(!HasSelection(field));
}

}  // namespace

int main() {
    MakeFieldStartsCollapsedAtEnd();
    FilterAcceptsOnlyItsOwnCharacters();
    InsertPutsTextAtTheCaret();
    InsertDropsRejectedCharactersButKeepsTheRest();
    InsertRefusesASecondSeparator();
    InsertStopsAtMaxLength();
    InsertReplacesTheSelection();
    SelectionSpansTheSmallerToLargerEnd();
    SelectAllThenTypingKeepsTheFieldValid();
    DeleteBackwardRemovesOneCharacterOrTheSelection();
    DeleteBackwardAtStartIsANoOp();
    DeleteForwardRemovesTheCharacterAfterTheCaret();
    DeleteSelectionCollapsesTheCaretToTheSelectionStart();
    MoveCaretClampsToTheText();
    MoveCaretWithoutExtendCollapsesTheSelection();
    ClearSelectionKeepsTheCaret();

    std::cout << "All OffWork text field tests passed.\n";
    return 0;
}
