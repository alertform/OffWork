#include "calculator.h"

#include <cassert>
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
    assert(working.phase == WorkdayPhase::Working);
    assert(working.remainingSeconds == 4 * 3600 + 30 * 60);
    assert(Near(working.earned, 450.0));
    assert(Near(working.progress, 0.5));

    const auto before = CalculateWorkday(8 * 3600, 9 * 60, 18 * 60, 600.0);
    assert(before.phase == WorkdayPhase::BeforeWork);
    assert(before.remainingSeconds == 3600);
    assert(Near(before.earned, 0.0));

    const auto after = CalculateWorkday(20 * 3600, 9 * 60, 18 * 60, 800.0);
    assert(after.phase == WorkdayPhase::AfterWork);
    assert(after.remainingSeconds == 0);
    assert(Near(after.earned, 800.0));

    const auto overnight = CalculateWorkday(1 * 3600, 22 * 60, 6 * 60, 800.0);
    assert(overnight.phase == WorkdayPhase::Working);
    assert(overnight.remainingSeconds == 5 * 3600);
    assert(Near(overnight.earned, 300.0));

    bool invalidHours = false;
    try {
        (void)CalculateWorkday(12 * 3600, 9 * 60, 9 * 60, 500.0);
    } catch (const std::invalid_argument&) {
        invalidHours = true;
    }
    assert(invalidHours);

    bool invalidSalary = false;
    try {
        (void)CalculateWorkday(12 * 3600, 9 * 60, 18 * 60, -1.0);
    } catch (const std::out_of_range&) {
        invalidSalary = true;
    }
    assert(invalidSalary);

    std::cout << "All OffWork calculation tests passed.\n";
    return 0;
}

