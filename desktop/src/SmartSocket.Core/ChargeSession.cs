using System.Globalization;

namespace SmartSocket.Core;

/// <summary>One completed charge, recorded at the moment power was cut.</summary>
public sealed record ChargeSession(
    long EndedAtMillis,
    int PeakMa,
    int CutAtMa,
    long DurationMs)
{
    public float PeakAmps => PeakMa / 1000f;

    /// <summary>
    /// How far the current fell before the socket cut, as a fraction of peak.
    /// The number the firmware's learning is built on, shown back to the user.
    /// </summary>
    public float TaperFraction =>
        PeakMa <= 0 ? 0f : Math.Clamp((float)CutAtMa / PeakMa, 0f, 1f);

    public string Encode() =>
        string.Create(
            CultureInfo.InvariantCulture,
            $"{EndedAtMillis},{PeakMa},{CutAtMa},{DurationMs}");

    public static ChargeSession? Decode(string? line)
    {
        if (line is null) return null;

        var p = line.Split(',');
        if (p.Length != 4) return null;

        if (!long.TryParse(p[0], NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out var endedAt)) return null;
        if (!int.TryParse(p[1], NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out var peak)) return null;
        if (!int.TryParse(p[2], NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out var cutAt)) return null;
        if (!long.TryParse(p[3], NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out var duration)) return null;

        return new ChargeSession(endedAt, peak, cutAt, duration);
    }
}
