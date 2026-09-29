#include "reminder.h"

#include "check.h"

#include <iostream>

namespace {

using offwork::CalculateWorkday;
using offwork::kReminderGraceSeconds;
using offwork::ReminderDecision;
using offwork::ReminderState;
using offwork::ResetReminder;
using offwork::SecondsSinceShiftEnd;
using offwork::TickReminder;
using offwork::WorkdayPhase;

constexpr int kDay = 24 * 60 * 60;

int At(int hours, int minutes, int seconds = 0) {
    return hours * 3600 + minutes * 60 + seconds;
}

// Feeds a run of ticks and returns how many of them asked for a reminder.
int CountNotifications(ReminderState& state, int fromSeconds, int toSeconds,
                       int startMinutes, int endMinutes) {
    int notified = 0;
    for (int t = fromSeconds; t < toSeconds; ++t) {
        const int now = t % kDay;
        const auto phase =
            CalculateWorkday(now, startMinutes, endMinutes, 500.0).phase;
        const ReminderDecision decision =
            TickReminder(state, phase, SecondsSinceShiftEnd(now, endMinutes));
        state = decision.next;
        if (decision.notify) {
            ++notified;
        }
    }
    return notified;
}

void SecondsSinceShiftEndWrapsAroundMidnight() {
    CHECK(SecondsSinceShiftEnd(At(18, 0), 18 * 60) == 0);
    CHECK(SecondsSinceShiftEnd(At(18, 5), 18 * 60) == 300);
    CHECK(SecondsSinceShiftEnd(At(17, 59), 18 * 60) == kDay - 60);
    CHECK(SecondsSinceShiftEnd(At(6, 0, 30), 6 * 60) == 30);
    CHECK(SecondsSinceShiftEnd(At(0, 10), 23 * 60 + 55) == 900);
}

void TransitionNotifiesExactlyOnce() {
    ReminderState state = ResetReminder(WorkdayPhase::Working);
    ReminderDecision d = TickReminder(state, WorkdayPhase::Working, kDay - 1);
    CHECK(!d.notify);
    d = TickReminder(d.next, WorkdayPhase::AfterWork, 0);
    CHECK(d.notify);
    d = TickReminder(d.next, WorkdayPhase::AfterWork, 1);
    CHECK(!d.notify);
    d = TickReminder(d.next, WorkdayPhase::AfterWork, 2);
    CHECK(!d.notify);
}

void RepeatedAfterWorkTicksNeverRepeat() {
    ReminderState state = ResetReminder(WorkdayPhase::Working);
    ReminderDecision d = TickReminder(state, WorkdayPhase::AfterWork, 0);
    CHECK(d.notify);
    int repeats = 0;
    for (int s = 1; s < 4 * 3600; ++s) {
        d = TickReminder(d.next, WorkdayPhase::AfterWork, s);
        repeats += d.notify ? 1 : 0;
    }
    CHECK(repeats == 0);
}

void StartupAfterWorkIsSilent() {
    const ReminderState state = ResetReminder(WorkdayPhase::AfterWork);
    CHECK(!TickReminder(state, WorkdayPhase::AfterWork, 5).notify);
    // Even right at the end moment: the app was not watching the transition.
    const ReminderState atEnd = ResetReminder(WorkdayPhase::AfterWork);
    CHECK(!TickReminder(atEnd, WorkdayPhase::AfterWork, 0).notify);
}

void FirstTickOfAnUninitialisedStateOnlyEstablishesABaseline() {
    const ReminderState blank;
    const ReminderDecision d = TickReminder(blank, WorkdayPhase::AfterWork, 0);
    CHECK(!d.notify);
    CHECK(d.next.initialized);
    CHECK(d.next.lastPhase == WorkdayPhase::AfterWork);
}

void ResetAfterSavingAScheduleIsSilent() {
    // Mid-shift, the user moves the end time to before "now": the phase
    // becomes AfterWork purely because of the edit, not because time passed.
    ReminderState state = ResetReminder(WorkdayPhase::Working);
    state = ResetReminder(WorkdayPhase::AfterWork);
    CHECK(!TickReminder(state, WorkdayPhase::AfterWork, 0).notify);

    // Reset while still working keeps the next real transition live.
    ReminderState working = ResetReminder(WorkdayPhase::Working);
    CHECK(TickReminder(working, WorkdayPhase::AfterWork, 1).notify);
}

void NextDayShiftRearms() {
    ReminderState state = ResetReminder(WorkdayPhase::Working);
    ReminderDecision d = TickReminder(state, WorkdayPhase::AfterWork, 0);
    CHECK(d.notify);
    d = TickReminder(d.next, WorkdayPhase::BeforeWork, 0);
    CHECK(!d.notify);
    d = TickReminder(d.next, WorkdayPhase::Working, 0);
    CHECK(!d.notify);
    d = TickReminder(d.next, WorkdayPhase::AfterWork, 0);
    CHECK(d.notify);
}

void OvernightShiftRearmsWithoutEverSeeingBeforeWork() {
    ReminderState state = ResetReminder(WorkdayPhase::Working);
    ReminderDecision d = TickReminder(state, WorkdayPhase::AfterWork, 0);
    CHECK(d.notify);
    d = TickReminder(d.next, WorkdayPhase::Working, 0);
    CHECK(!d.notify);
    d = TickReminder(d.next, WorkdayPhase::AfterWork, 0);
    CHECK(d.notify);
}

void GraceWindowBoundaryIsInclusive() {
    const ReminderState state = ResetReminder(WorkdayPhase::Working);
    CHECK(TickReminder(state, WorkdayPhase::AfterWork, kReminderGraceSeconds).notify);
    CHECK(!TickReminder(state, WorkdayPhase::AfterWork, kReminderGraceSeconds + 1).notify);
}

void LateObservationIsSilentAndDoesNotFireLater() {
    // Asleep from 17:50 until 19:40.
    ReminderState state = ResetReminder(WorkdayPhase::Working);
    ReminderDecision d = TickReminder(state, WorkdayPhase::AfterWork, 100 * 60);
    CHECK(!d.notify);
    d = TickReminder(d.next, WorkdayPhase::AfterWork, 100 * 60 + 1);
    CHECK(!d.notify);
}

void SleepingThroughTheWholeShiftIsSilent() {
    ReminderState state = ResetReminder(WorkdayPhase::BeforeWork);
    CHECK(!TickReminder(state, WorkdayPhase::AfterWork, 0).notify);
}

void SimulatedDayShiftNotifiesOnceAtTheEndSecond() {
    ReminderState state = ResetReminder(
        CalculateWorkday(At(8, 0), 9 * 60, 18 * 60, 500.0).phase);
    CHECK(CountNotifications(state, At(8, 0), At(17, 59, 59), 9 * 60, 18 * 60) == 0);
    CHECK(CountNotifications(state, At(17, 59, 59), At(18, 0, 1), 9 * 60, 18 * 60) == 1);
    CHECK(CountNotifications(state, At(18, 0, 1), At(23, 59, 59), 9 * 60, 18 * 60) == 0);
}

void TwoSimulatedDaysNotifyTwice() {
    ReminderState state = ResetReminder(
        CalculateWorkday(At(7, 0), 9 * 60, 18 * 60, 500.0).phase);
    CHECK(CountNotifications(state, At(7, 0), At(7, 0) + 2 * kDay, 9 * 60, 18 * 60) == 2);
}

void SimulatedOvernightShiftNotifiesAtSixInTheMorning() {
    ReminderState state = ResetReminder(
        CalculateWorkday(At(21, 0), 22 * 60, 6 * 60, 800.0).phase);
    CHECK(CountNotifications(state, At(21, 0), At(21, 0) + kDay, 22 * 60, 6 * 60) == 1);
}

}  // namespace

int main() {
    SecondsSinceShiftEndWrapsAroundMidnight();
    TransitionNotifiesExactlyOnce();
    RepeatedAfterWorkTicksNeverRepeat();
    StartupAfterWorkIsSilent();
    FirstTickOfAnUninitialisedStateOnlyEstablishesABaseline();
    ResetAfterSavingAScheduleIsSilent();
    NextDayShiftRearms();
    OvernightShiftRearmsWithoutEverSeeingBeforeWork();
    GraceWindowBoundaryIsInclusive();
    LateObservationIsSilentAndDoesNotFireLater();
    SleepingThroughTheWholeShiftIsSilent();
    SimulatedDayShiftNotifiesOnceAtTheEndSecond();
    TwoSimulatedDaysNotifyTwice();
    SimulatedOvernightShiftNotifiesAtSixInTheMorning();

    std::cout << "All OffWork reminder tests passed.\n";
    return 0;
}
