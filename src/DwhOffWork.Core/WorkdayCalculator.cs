namespace DwhOffWork.Core;

public enum WorkdayPhase
{
    BeforeWork,
    Working,
    AfterWork,
}

public sealed record WorkdaySnapshot(
    WorkdayPhase Phase,
    DateTime ShiftStart,
    DateTime ShiftEnd,
    TimeSpan Remaining,
    decimal Earned,
    decimal DailySalary,
    double Progress);

public static class WorkdayCalculator
{
    public static WorkdaySnapshot Calculate(
        DateTime now,
        TimeOnly startTime,
        TimeOnly endTime,
        decimal dailySalary)
    {
        if (dailySalary < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(dailySalary));
        }

        if (startTime == endTime)
        {
            throw new ArgumentException("Start and end times must be different.");
        }

        var shiftStart = now.Date.Add(startTime.ToTimeSpan());
        var spansMidnight = endTime < startTime;
        var shiftEnd = spansMidnight
            ? now.Date.AddDays(1).Add(endTime.ToTimeSpan())
            : now.Date.Add(endTime.ToTimeSpan());

        if (spansMidnight)
        {
            var previousStart = shiftStart.AddDays(-1);
            var previousEnd = now.Date.Add(endTime.ToTimeSpan());

            if (now < shiftStart)
            {
                shiftStart = previousStart;
                shiftEnd = previousEnd;
            }
        }

        var shiftDuration = shiftEnd - shiftStart;
        WorkdayPhase phase;
        TimeSpan elapsed;
        TimeSpan remaining;

        if (now < shiftStart)
        {
            phase = WorkdayPhase.BeforeWork;
            elapsed = TimeSpan.Zero;
            remaining = shiftStart - now;
        }
        else if (now < shiftEnd)
        {
            phase = WorkdayPhase.Working;
            elapsed = now - shiftStart;
            remaining = shiftEnd - now;
        }
        else
        {
            phase = WorkdayPhase.AfterWork;
            elapsed = shiftDuration;
            remaining = TimeSpan.Zero;
        }

        var progress = Math.Clamp(elapsed.TotalSeconds / shiftDuration.TotalSeconds, 0, 1);
        var earned = decimal.Round(dailySalary * (decimal)progress, 2, MidpointRounding.AwayFromZero);

        return new WorkdaySnapshot(
            phase,
            shiftStart,
            shiftEnd,
            remaining,
            earned,
            dailySalary,
            progress);
    }
}

