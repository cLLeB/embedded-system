using System.Globalization;

namespace SmartSocket.Core;

/// <summary>
/// Decodes one line of the socket's wire format.
///
/// <code>
/// S,&lt;state&gt;,&lt;mA&gt;,&lt;peakMa&gt;,&lt;thresholdMa&gt;,&lt;elapsedMs&gt;,&lt;cutoffs&gt;,&lt;savedMs&gt;,&lt;relay&gt;
/// </code>
///
/// Returns null for anything it does not fully understand rather than guessing.
/// A serial link drops mid-line whenever the socket is reset or a Bluetooth
/// virtual port goes out of range, so partial and corrupt lines are the normal
/// case, not an exceptional one - and a half-parsed line would show the user a
/// number that never existed.
///
/// This is a port of <c>StatusParser.kt</c>. The two must agree field for
/// field; if they ever disagree, one of the two clients is showing a number
/// that was never measured, which is the one failure a socket switching mains
/// must not have.
/// </summary>
public static class StatusParser
{
    private const int FieldCount = 9;

    public static SocketStatus? Parse(string? line)
    {
        if (line is null) return null;

        var trimmed = line.Trim();
        if (!trimmed.StartsWith("S,", StringComparison.Ordinal)) return null;

        var parts = trimmed.Split(',');
        if (parts.Length != FieldCount) return null;

        if (!TryInt(parts[1], out var ordinal)) return null;
        if (SocketStateExtensions.FromOrdinal(ordinal) is not { } state) return null;

        if (!TryInt(parts[2], out var currentMa)) return null;
        if (!TryInt(parts[3], out var peakMa)) return null;
        if (!TryInt(parts[4], out var thresholdMa)) return null;
        if (!TryLong(parts[5], out var elapsedMs)) return null;
        if (!TryInt(parts[6], out var cutoffCount)) return null;
        if (!TryLong(parts[7], out var savedMs)) return null;

        return new SocketStatus(
            State: state,
            CurrentMa: currentMa,
            PeakMa: peakMa,
            ThresholdMa: thresholdMa,
            SessionElapsedMs: elapsedMs,
            CutoffCount: cutoffCount,
            TotalSavedMs: savedMs,
            // A flag, not a truthy number. Anything that is not exactly "1" is
            // open, because a relay reported closed when it is not is the
            // dangerous direction to be wrong in.
            RelayClosed: parts[8].Trim() == "1");
    }

    // Invariant culture on purpose: the firmware prints ASCII digits, and a
    // machine with a comma decimal separator must not reinterpret them.
    private static bool TryInt(string s, out int value) =>
        int.TryParse(s, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out value);

    private static bool TryLong(string s, out long value) =>
        long.TryParse(s, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out value);
}
