#include "reminder.h"

namespace offwork {
namespace {

constexpr int kSecondsPerDay = 24 * 60 * 60;

}  // namespace

int SecondsSinceShiftEnd(int nowSeconds, int endMinutes) {
    const int delta = (nowSeconds - endMinutes * 60) % kSecondsPerDay;
    return delta < 0 ? delta + kSecondsPerDay : delta;
}

ReminderState ResetReminder(WorkdayPhase currentPhase) {
    ReminderState state;
    state.initialized = true;
    state.lastPhase = currentPhase;
    return state;
}

ReminderDecision TickReminder(
    const ReminderState& state, WorkdayPhase phase, int secondsSinceEnd) {
    ReminderDecision decision;
    decision.next = ResetReminder(phase);
    if (!state.initialized) {
        return decision;
    }
    // Rearming is implicit: any tick that sees Working makes the next
    // AfterWork a transition again. Overnight shifts never report BeforeWork,
    // so rearming must not depend on it.
    decision.notify = state.lastPhase == WorkdayPhase::Working &&
                      phase == WorkdayPhase::AfterWork &&
                      secondsSinceEnd <= kReminderGraceSeconds;
    return decision;
}

}  // namespace offwork
