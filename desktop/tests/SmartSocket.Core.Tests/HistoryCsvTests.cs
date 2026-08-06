using SmartSocket.Core;
using Xunit;

namespace SmartSocket.Core.Tests;

public class HistoryCsvTests
{
    private static readonly TimeZoneInfo Utc = TimeZoneInfo.Utc;

    private static ChargeSession Session(
        long endedAtMillis,
        int peakMa = 150,
        int cutAtMa = 60,
        long durationMs = 3_600_000) =>
        new(endedAtMillis, peakMa, cutAtMa, durationMs);

    private static string[] Lines(string csv) => csv.Split('\n');

    [Fact]
    public void An_empty_history_is_still_a_readable_file_not_a_blank_one()
    {
        Assert.Equal(HistoryCsv.Header, HistoryCsv.Render([], Utc));
    }

    [Fact]
    public void Every_session_is_one_row_under_one_header()
    {
        var csv = HistoryCsv.Render([Session(2_000), Session(1_000), Session(3_000)], Utc);

        Assert.Equal(4, Lines(csv).Length);
        Assert.Equal(HistoryCsv.Header, Lines(csv)[0]);
    }

    /// <summary>
    /// The store keeps newest first for the screen. A spreadsheet plotting
    /// these needs the opposite, and silently plotting time backwards is the
    /// kind of error nobody notices.
    /// </summary>
    [Fact]
    public void Rows_run_oldest_first_whatever_order_they_arrive_in()
    {
        var csv = HistoryCsv.Render([Session(3_000), Session(1_000), Session(2_000)], Utc);

        var stamps = Lines(csv).Skip(1).Select(l => long.Parse(l.Split(',')[1])).ToArray();
        Assert.Equal([1_000L, 2_000L, 3_000L], stamps);
    }

    /// <summary>
    /// The same instant and the same zone must render the same string as the
    /// Android app does, or two exports of the same charge disagree.
    /// </summary>
    [Fact]
    public void The_human_column_is_a_real_date_in_the_machines_zone()
    {
        var csv = HistoryCsv.Render([Session(1_700_000_000_000)], Utc);
        Assert.Equal("2023-11-14 22:13:20", Lines(csv)[1].Split(',')[0]);
    }

    [Fact]
    public void Columns_match_the_header_in_order()
    {
        var csv = HistoryCsv.Render(
            [Session(1_000, peakMa: 160, cutAtMa: 40, durationMs: 90_000)],
            Utc);

        var row = Lines(csv)[1].Split(',');

        Assert.Equal(HistoryCsv.Header.Split(',').Length, row.Length);
        Assert.Equal("1000", row[1]);
        Assert.Equal("160", row[2]);
        Assert.Equal("40", row[3]);
        Assert.Equal("90000", row[4]);
        Assert.Equal("25", row[5]);
    }

    [Fact]
    public void A_session_with_no_peak_does_not_divide_by_zero()
    {
        var csv = HistoryCsv.Render([Session(1_000, peakMa: 0, cutAtMa: 0)], Utc);
        Assert.Equal("0", Lines(csv)[1].Split(',')[5]);
    }

    /// <summary>
    /// Nothing written here is user-supplied text, so there is nothing to quote
    /// - but if a field ever gains a comma the column count silently shifts and
    /// every row after it is misread.
    /// </summary>
    [Fact]
    public void No_field_smuggles_in_a_separator()
    {
        var csv = HistoryCsv.Render([Session(1_000), Session(2_000)], Utc);
        var columns = HistoryCsv.Header.Split(',').Length;

        foreach (var line in Lines(csv))
        {
            Assert.True(
                line.Split(',').Length == columns,
                $"`{line}` has the wrong column count");
        }
    }
}
