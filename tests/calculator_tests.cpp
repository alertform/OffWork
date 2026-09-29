#include "calculator.h"

#include "check.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool Near(double left, double right, double epsilon = 0.0001) {
    return std::abs(left - right) < epsilon;
}

}  // namespace

int main() {
    using offwork::CalculateWorkday;
    using offwork::DurationMinutesFromHours;
    using offwork::EndMinutesFromDuration;
    using offwork::WorkDurationMinutes;
    using offwork::WorkdayPhase;

    CHECK(EndMinutesFromDuration(9 * 60, 8 * 60) == 17 * 60);
    CHECK(EndMinutesFromDuration(9 * 60, 9 * 60) == 18 * 60);
    CHECK(EndMinutesFromDuration(22 * 60, 8 * 60) == 6 * 60);
    CHECK(WorkDurationMinutes(9 * 60, 17 * 60) == 8 * 60);
    CHECK(WorkDurationMinutes(22 * 60, 6 * 60) == 8 * 60);
    CHECK(DurationMinutesFromHours(8.0) == 8 * 60);
    CHECK(DurationMinutesFromHours(8.5) == 8 * 60 + 30);
    CHECK(DurationMinutesFromHours(9.0) == 9 * 60);

    bool invalidDuration = false;
    try {
        (void)EndMinutesFromDuration(9 * 60, 24 * 60);
    } catch (const std::invalid_argument&) {
        invalidDuration = true;
    }
    CHECK(invalidDuration);

    bool invalidDurationHours = false;
    try {
        (void)DurationMinutesFromHours(24.0);
    } catch (const std::invalid_argument&) {
        invalidDurationHours = true;
    }
    CHECK(invalidDurationHours);

    const auto working = CalculateWorkday(13 * 3600 + 30 * 60, 9 * 60, 18 * 60, 900.0);
    CHECK(working.phase == WorkdayPhase::Working);
    CHECK(working.remainingSeconds == 4 * 3600 + 30 * 60);
    CHECK(Near(working.earned, 450.0));
    CHECK(Near(working.progress, 0.5));

    const auto before = CalculateWorkday(8 * 3600, 9 * 60, 18 * 60, 600.0);
    CHECK(before.phase == WorkdayPhase::BeforeWork);
    CHECK(before.remainingSeconds == 3600);
    CHECK(Near(before.earned, 0.0));

    const auto after = CalculateWorkday(20 * 3600, 9 * 60, 18 * 60, 800.0);
    CHECK(after.phase == WorkdayPhase::AfterWork);
    CHECK(after.remainingSeconds == 0);
    CHECK(Near(after.earned, 800.0));

    const auto overnight = CalculateWorkday(1 * 3600, 22 * 60, 6 * 60, 800.0);
    CHECK(overnight.phase == WorkdayPhase::Working);
    CHECK(overnight.remainingSeconds == 5 * 3600);
    CHECK(Near(overnight.earned, 300.0));

    bool invalidHours = false;
    try {
        (void)CalculateWorkday(12 * 3600, 9 * 60, 9 * 60, 500.0);
    } catch (const std::invalid_argument&) {
        invalidHours = true;
    }
    CHECK(invalidHours);

    bool invalidSalary = false;
    try {
        (void)CalculateWorkday(12 * 3600, 9 * 60, 18 * 60, -1.0);
    } catch (const std::out_of_range&) {
        invalidSalary = true;
    }
    CHECK(invalidSalary);

    std::cout << "All OffWork calculation tests passed.\n";
    return 0;
}
