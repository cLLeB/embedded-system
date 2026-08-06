namespace SmartSocket.Core;

/// <summary>
/// How long to wait before the nth attempt to rebuild a dropped link.
///
/// Pure arithmetic with no timers and no I/O in it, so the schedule can be
/// asserted in a unit test rather than discovered by unplugging a USB cable
/// and counting. <c>SocketRepository</c> supplies the clock and the port.
///
/// The shape is exponential up to a one-minute ceiling. The two cases it has to
/// serve pull in opposite directions: a dropped USB enumeration wants a retry
/// now, and a laptop carried out of Bluetooth range wants one in ten minutes'
/// time. Doubling from two seconds covers the first in the first few attempts,
/// and the ceiling stops the rest from costing anything.
///
/// It gives up rather than retrying forever. A tray icon claiming to watch a
/// socket that is switched off at the wall is worse than an honest "lost it".
/// </summary>
public static class ReconnectPolicy
{
    /// <summary>Roughly eight minutes of trying, in total.</summary>
    public const int MaxAttempts = 12;

    private const long FirstDelayMs = 2_000L;
    private const long CeilingMs = 60_000L;

    /// <summary>
    /// Wait before <paramref name="attempt"/>, counted from zero, or null once
    /// the schedule is exhausted and the caller should stop.
    /// </summary>
    public static long? DelayMsFor(int attempt)
    {
        if (attempt < 0 || attempt >= MaxAttempts) return null;
        return Math.Min(FirstDelayMs << attempt, CeilingMs);
    }

    /// <summary>Total time the schedule spends waiting before it gives up.</summary>
    public static long BudgetMs
    {
        get
        {
            long total = 0;
            for (var i = 0; i < MaxAttempts; i++) total += DelayMsFor(i) ?? 0L;
            return total;
        }
    }
}
