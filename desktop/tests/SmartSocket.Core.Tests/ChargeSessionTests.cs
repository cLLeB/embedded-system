using SmartSocket.Core;
using Xunit;

namespace SmartSocket.Core.Tests;

public class ChargeSessionTests
{
    [Fact]
    public void A_session_survives_a_round_trip_through_the_file()
    {
        var session = new ChargeSession(
            EndedAtMillis: 1_700_000_000_000,
            PeakMa: 158,
            CutAtMa: 62,
            DurationMs: 4_512_000);

        Assert.Equal(session, ChargeSession.Decode(session.Encode()));
    }

    [Fact]
    public void A_corrupt_line_is_dropped_rather_than_half_read()
    {
        Assert.Null(ChargeSession.Decode(""));
        Assert.Null(ChargeSession.Decode("1,2,3"));
        Assert.Null(ChargeSession.Decode("1,2,3,4,5"));
        Assert.Null(ChargeSession.Decode("1,2,3,x"));
        Assert.Null(ChargeSession.Decode(null));
    }

    [Fact]
    public void Taper_is_the_fraction_of_peak_the_current_fell_to()
    {
        var session = new ChargeSession(0, PeakMa: 150, CutAtMa: 60, DurationMs: 0);
        Assert.Equal(0.4f, session.TaperFraction, 3);
    }

    /// <summary>
    /// A cutoff recorded before any peak was seen would otherwise divide by
    /// zero. It happens on the trickle-rejection path, where the socket cuts a
    /// load it never saw charge.
    /// </summary>
    [Fact]
    public void A_session_with_no_peak_reports_no_taper_instead_of_dividing_by_zero()
    {
        var session = new ChargeSession(0, PeakMa: 0, CutAtMa: 0, DurationMs: 0);
        Assert.Equal(0f, session.TaperFraction, 3);
    }

    [Fact]
    public void A_cut_above_the_peak_is_clamped_rather_than_shown_over_one_hundred_percent()
    {
        var session = new ChargeSession(0, PeakMa: 100, CutAtMa: 180, DurationMs: 0);
        Assert.Equal(1f, session.TaperFraction, 3);
    }

    /// <summary>
    /// The encoded form is written to disk and is the same format the Android
    /// app uses, so a history file is portable between the two.
    /// </summary>
    [Fact]
    public void Encoding_is_four_plain_fields()
    {
        var session = new ChargeSession(1_000, 160, 40, 90_000);
        Assert.Equal("1000,160,40,90000", session.Encode());
    }
}
