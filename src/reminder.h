#pragma once

// Off-work reminder decision. Pure: no Windows headers, no clock, no I/O, so
// every transition can be tested deterministically. The window feeds it one
// phase per clock tick and shows a balloon when it says so.
//
// Every operation returns a new ReminderState instead of mutating its input.

#include "calculator.h"

namespace offwork {

// A transition first observed later than this after the end time is treated
// as stale and stays silent (machine asleep or locked across the end time,
// clock jumps). User decision, 2026-09-29. Boundary inclusive.
inline constexpr int kReminderGraceSeconds = 10 * 60;

struct ReminderState {
    bool initialized = false;
    WorkdayPhase lastPhase = WorkdayPhase::AfterWork;
};

struct ReminderDecision {
    ReminderState next;
    bool notify = false;
};

// Seconds since the most recent end-of-shift moment, in [0, 86400).
int SecondsSinceShiftEnd(int nowSeconds, int endMinutes);

// Baseline without notifying: at startup, and whenever the schedule changes.
ReminderState ResetReminder(WorkdayPhase currentPhase);

// One clock tick. Notifies exactly once per Working -> AfterWork transition,
// and only when that transition is seen within the grace window.
ReminderDecision TickReminder(
    const ReminderState& state, WorkdayPhase phase, int secondsSinceEnd);

}  // namespace offwork
