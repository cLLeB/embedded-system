using System.Globalization;
using System.Text;

namespace SmartSocket.Core;

/// <summary>
/// Charge history as a spreadsheet, oldest first.
///
/// Oldest first even though the store keeps newest first: on screen the newest
/// charge belongs at the top, but a column of timestamps that a spreadsheet is
/// going to plot has to run forwards.
///
/// Two time columns on purpose. The readable one is for a human opening the
/// file, the millisecond one is for a spreadsheet that will not parse it - and
/// neither can be reconstructed from the other without knowing which zone the
/// machine was in.
///
/// Byte-for-byte the same format as the Android app's <c>HistoryCsv.kt</c>, so
/// exports from a phone and a laptop can be concatenated.
/// </summary>
public static class HistoryCsv
{
    public const string Header =
        "ended_at,ended_at_ms,peak_ma,cut_at_ma,duration_ms,taper_percent";

    public const string FileName = "smart-socket-history.csv";

    public static string Render(
        IEnumerable<ChargeSession> sessions,
        TimeZoneInfo? zone = null)
    {
        zone ??= TimeZoneInfo.Local;

        var builder = new StringBuilder(Header);

        foreach (var session in sessions.OrderBy(s => s.EndedAtMillis))
        {
            var local = TimeZoneInfo.ConvertTime(
                DateTimeOffset.FromUnixTimeMilliseconds(session.EndedAtMillis),
                zone);

            builder.Append('\n');
            builder.Append(local.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture));
            builder.Append(',');
            builder.Append(session.EndedAtMillis.ToString(CultureInfo.InvariantCulture));
            builder.Append(',');
            builder.Append(session.PeakMa.ToString(CultureInfo.InvariantCulture));
            builder.Append(',');
            builder.Append(session.CutAtMa.ToString(CultureInfo.InvariantCulture));
            builder.Append(',');
            builder.Append(session.DurationMs.ToString(CultureInfo.InvariantCulture));
            builder.Append(',');
            builder.Append(((int)(session.TaperFraction * 100)).ToString(CultureInfo.InvariantCulture));
        }

        return builder.ToString();
    }
}
