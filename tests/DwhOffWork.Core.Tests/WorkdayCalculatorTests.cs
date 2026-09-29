using DwhOffWork.Core;
using Xunit;

namespace DwhOffWork.Core.Tests;

public sealed class WorkdayCalculatorTests
{
    [Fact]
    public void DuringWork_CalculatesProgressEarningsAndCountdown()
    {
        var snapshot = WorkdayCalculator.Calculate(
            new DateTime(2026, 9, 29, 13, 30, 0),
            new TimeOnly(9, 0),
            new TimeOnly(18, 0),
            900m);

        Assert.Equal(WorkdayPhase.Working, snapshot.Phase);
        Assert.Equal(TimeSpan.FromHours(4.5), snapshot.Remaining);
        Assert.Equal(450m, snapshot.Earned);
        Assert.Equal(0.5, snapshot.Progress, 5);
    }

    [Fact]
    public void BeforeWork_ShowsNoEarnings()
    {
        var snapshot = WorkdayCalculator.Calculate(
            new DateTime(2026, 9, 29, 8, 0, 0),
            new TimeOnly(9, 0),
            new TimeOnly(18, 0),
            600m);

        Assert.Equal(WorkdayPhase.BeforeWork, snapshot.Phase);
        Assert.Equal(TimeSpan.FromHours(1), snapshot.Remaining);
        Assert.Equal(0m, snapshot.Earned);
        Assert.Equal(0, snapshot.Progress);
    }

    [Fact]
    public void AfterWork_CapsEarningsAtDailySalary()
    {
        var snapshot = WorkdayCalculator.Calculate(
            new DateTime(2026, 9, 29, 20, 0, 0),
            new TimeOnly(9, 0),
            new TimeOnly(18, 0),
            800m);

        Assert.Equal(WorkdayPhase.AfterWork, snapshot.Phase);
        Assert.Equal(TimeSpan.Zero, snapshot.Remaining);
        Assert.Equal(800m, snapshot.Earned);
        Assert.Equal(1, snapshot.Progress);
    }

    [Fact]
    public void OvernightShift_UsesPreviousDayStartAfterMidnight()
    {
        var snapshot = WorkdayCalculator.Calculate(
            new DateTime(2026, 9, 30, 1, 0, 0),
            new TimeOnly(22, 0),
            new TimeOnly(6, 0),
            800m);

        Assert.Equal(WorkdayPhase.Working, snapshot.Phase);
        Assert.Equal(new DateTime(2026, 9, 29, 22, 0, 0), snapshot.ShiftStart);
        Assert.Equal(TimeSpan.FromHours(5), snapshot.Remaining);
        Assert.Equal(300m, snapshot.Earned);
    }

    [Fact]
    public void InvalidInputs_AreRejected()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => WorkdayCalculator.Calculate(
            DateTime.Now,
            new TimeOnly(9, 0),
            new TimeOnly(18, 0),
            -1m));

        Assert.Throws<ArgumentException>(() => WorkdayCalculator.Calculate(
            DateTime.Now,
            new TimeOnly(9, 0),
            new TimeOnly(9, 0),
            500m));
    }
}
