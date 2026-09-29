#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace offwork {

enum class WorkdayPhase {
    BeforeWork,
    Working,
    AfterWork,
};

struct WorkdaySnapshot {
    WorkdayPhase phase;
    int remainingSeconds;
    double earned;
    double progress;
};

inline int WorkDurationMinutes(int startMinutes, int endMinutes) {
    constexpr int minutesPerDay = 24 * 60;
    if (startMinutes < 0 || startMinutes >= minutesPerDay ||
        endMinutes < 0 || endMinutes >= minutesPerDay ||
        startMinutes == endMinutes) {
        throw std::invalid_argument("work hours");
    }
    return endMinutes > startMinutes
        ? endMinutes - startMinutes
        : minutesPerDay - startMinutes + endMinutes;
}

inline int EndMinutesFromDuration(int startMinutes, int durationMinutes) {
    constexpr int minutesPerDay = 24 * 60;
    if (startMinutes < 0 || startMinutes >= minutesPerDay) {
        throw std::invalid_argument("start time");
    }
    if (durationMinutes <= 0 || durationMinutes >= minutesPerDay) {
        throw std::invalid_argument("work duration");
    }
    return (startMinutes + durationMinutes) % minutesPerDay;
}

inline int DurationMinutesFromHours(double hours) {
    if (!std::isfinite(hours) || hours <= 0.0 || hours >= 24.0) {
        throw std::invalid_argument("work duration hours");
    }
    const int minutes = static_cast<int>(std::round(hours * 60.0));
    if (minutes <= 0 || minutes >= 24 * 60) {
        throw std::invalid_argument("work duration hours");
    }
    return minutes;
}

inline WorkdaySnapshot CalculateWorkday(
    int nowSeconds,
    int startMinutes,
    int endMinutes,
    double dailySalary) {
    constexpr int secondsPerDay = 24 * 60 * 60;

    if (nowSeconds < 0 || nowSeconds >= secondsPerDay) {
        throw std::out_of_range("nowSeconds");
    }
    if (startMinutes < 0 || startMinutes >= 24 * 60 ||
        endMinutes < 0 || endMinutes >= 24 * 60 ||
        startMinutes == endMinutes) {
        throw std::invalid_argument("work hours");
    }
    if (dailySalary < 0) {
        throw std::out_of_range("dailySalary");
    }

    const int startSeconds = startMinutes * 60;
    const int endSeconds = endMinutes * 60;
    const bool spansMidnight = endSeconds < startSeconds;
    const int duration = spansMidnight
        ? secondsPerDay - startSeconds + endSeconds
        : endSeconds - startSeconds;

    WorkdayPhase phase = WorkdayPhase::AfterWork;
    int elapsed = duration;
    int remaining = 0;

    if (!spansMidnight) {
        if (nowSeconds < startSeconds) {
            phase = WorkdayPhase::BeforeWork;
            elapsed = 0;
            remaining = startSeconds - nowSeconds;
        } else if (nowSeconds < endSeconds) {
            phase = WorkdayPhase::Working;
            elapsed = nowSeconds - startSeconds;
            remaining = endSeconds - nowSeconds;
        }
    } else if (nowSeconds >= startSeconds || nowSeconds < endSeconds) {
        phase = WorkdayPhase::Working;
        elapsed = nowSeconds >= startSeconds
            ? nowSeconds - startSeconds
            : secondsPerDay - startSeconds + nowSeconds;
        remaining = duration - elapsed;
    }

    const double progress = std::clamp(
        static_cast<double>(elapsed) / static_cast<double>(duration), 0.0, 1.0);
    const double earned = dailySalary * progress;
    return {phase, remaining, earned, progress};
}

}  // namespace offwork
