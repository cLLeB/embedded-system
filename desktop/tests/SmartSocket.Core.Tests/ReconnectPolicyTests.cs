using SmartSocket.Core;
using Xunit;

namespace SmartSocket.Core.Tests;

public class ReconnectPolicyTests
{
    [Fact]
    public void First_retry_is_quick_enough_to_cover_a_dropped_enumeration()
    {
        Assert.Equal(2_000L, ReconnectPolicy.DelayMsFor(0));
    }

    [Fact]
    public void Waits_double_until_they_hit_the_ceiling()
    {
        Assert.Equal(2_000L, ReconnectPolicy.DelayMsFor(0));
        Assert.Equal(4_000L, ReconnectPolicy.DelayMsFor(1));
        Assert.Equal(8_000L, ReconnectPolicy.DelayMsFor(2));
        Assert.Equal(16_000L, ReconnectPolicy.DelayMsFor(3));
        Assert.Equal(32_000L, ReconnectPolicy.DelayMsFor(4));
    }

    [Fact]
    public void Never_waits_more_than_a_minute()
    {
        for (var attempt = 0; attempt < ReconnectPolicy.MaxAttempts; attempt++)
        {
            var delay = ReconnectPolicy.DelayMsFor(attempt);
            Assert.True(delay is not null, $"attempt {attempt} should have a delay");
            Assert.True(delay!.Value <= 60_000L, $"attempt {attempt} waited {delay}ms");
        }
    }

    [Fact]
    public void Gives_up_rather_than_retrying_forever()
    {
        Assert.Null(ReconnectPolicy.DelayMsFor(ReconnectPolicy.MaxAttempts));
        Assert.Null(ReconnectPolicy.DelayMsFor(ReconnectPolicy.MaxAttempts + 50));
    }

    [Fact]
    public void A_negative_attempt_is_refused_rather_than_shifted_by_a_negative_amount()
    {
        Assert.Null(ReconnectPolicy.DelayMsFor(-1));
    }

    /// <summary>
    /// The point of the ceiling. Someone who carries a laptop out of range and
    /// comes back a few minutes later should still find the socket connected,
    /// so the budget has to be minutes rather than the seconds a plain
    /// exponential would give.
    /// </summary>
    [Fact]
    public void Keeps_trying_for_several_minutes_before_it_gives_up()
    {
        var budget = ReconnectPolicy.BudgetMs;
        Assert.True(
            budget >= 5 * 60_000L && budget <= 15 * 60_000L,
            $"budget was {budget}ms");
    }

    /// <summary>
    /// The schedule is the app's contract with the user, and the Android app
    /// promises the same one. A change here that is not mirrored there means
    /// two clients behave differently on the same failure.
    /// </summary>
    [Fact]
    public void Matches_the_android_schedule()
    {
        Assert.Equal(12, ReconnectPolicy.MaxAttempts);
    }
}
