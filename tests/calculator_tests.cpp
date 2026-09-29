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
    using offwork::WorkdayPhase;

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

