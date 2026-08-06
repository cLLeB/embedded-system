using SmartSocket.Core;
using Xunit;

namespace SmartSocket.Core.Tests;

/// <summary>
/// The contract with <c>HalTelemetry::publish</c>. Ported case for case from
/// the Android app's StatusParserTest, so the two clients cannot drift apart
/// silently. If these and the firmware ever disagree, the app shows a number
/// that was never measured - the one failure mode a socket that switches mains
/// must not have.
/// </summary>
public class StatusParserTests
{
    /// <summary>Copied from a real Serial.print run, not invented.</summary>
    private const string Line = "S,3,142,158,100,45230,3,1800000,1";

    [Fact]
    public void A_real_status_line_decodes_field_for_field()
    {
        var status = StatusParser.Parse(Line);

        Assert.NotNull(status);
        Assert.Equal(SocketState.Charging, status!.State);
        Assert.Equal(142, status.CurrentMa);
        Assert.Equal(158, status.PeakMa);
        Assert.Equal(100, status.ThresholdMa);
        Assert.Equal(45_230L, status.SessionElapsedMs);
        Assert.Equal(3, status.CutoffCount);
        Assert.Equal(1_800_000L, status.TotalSavedMs);
        Assert.True(status.RelayClosed);
    }

    [Fact]
    public void The_relay_flag_is_a_flag_not_a_truthy_number()
    {
        Assert.False(StatusParser.Parse("S,4,0,0,0,0,3,1800000,0")!.RelayClosed);
        Assert.True(StatusParser.Parse("S,4,0,0,0,0,3,1800000,1")!.RelayClosed);
        Assert.False(StatusParser.Parse("S,4,0,0,0,0,3,1800000,2")!.RelayClosed);
    }

    [Fact]
    public void A_line_ending_survives_the_trip()
    {
        Assert.NotNull(StatusParser.Parse(Line + "\r"));
        Assert.NotNull(StatusParser.Parse("  " + Line + "  "));
        Assert.NotNull(StatusParser.Parse(Line + "\r\n"));
    }

    /// <summary>
    /// A serial link drops mid-line every time the socket resets, so a
    /// truncated line is the normal case. Half a status is not a status.
    /// </summary>
    [Fact]
    public void A_truncated_line_is_rejected_rather_than_half_read()
    {
        Assert.Null(StatusParser.Parse("S,3,142,158"));
        Assert.Null(StatusParser.Parse("S,3,142,158,100,45230,3,1800000"));
    }

    [Fact]
    public void Extra_fields_are_rejected_not_ignored()
    {
        Assert.Null(StatusParser.Parse(Line + ",99"));
    }

    [Fact]
    public void Noise_on_the_wire_is_not_a_status()
    {
        Assert.Null(StatusParser.Parse(""));
        Assert.Null(StatusParser.Parse("OK"));
        Assert.Null(StatusParser.Parse("+CONNECTED"));
        Assert.Null(StatusParser.Parse("3,142,158,100,45230,3,1800000,1"));
        Assert.Null(StatusParser.Parse(null));
    }

    [Fact]
    public void A_field_that_is_not_a_number_is_not_guessed_at()
    {
        Assert.Null(StatusParser.Parse("S,3,xx,158,100,45230,3,1800000,1"));
        Assert.Null(StatusParser.Parse("S,3,142,158,100,45230,3,,1"));
    }

    /// <summary>
    /// The firmware sends the enum ordinal. An ordinal this list does not know
    /// means the two sides have drifted apart, and inventing a state for it
    /// would hide exactly that.
    /// </summary>
    [Fact]
    public void An_unknown_state_ordinal_is_refused()
    {
        Assert.Null(StatusParser.Parse("S,99,0,0,0,0,0,0,0"));
        Assert.Null(StatusParser.Parse("S,-1,0,0,0,0,0,0,0"));
        Assert.Null(StatusParser.Parse("S,9,0,0,0,0,0,0,0"));
    }

    [Fact]
    public void Every_ordinal_the_firmware_can_send_maps_to_a_state()
    {
        foreach (SocketState expected in Enum.GetValues<SocketState>())
        {
            var ordinal = (int)expected;
            var status = StatusParser.Parse($"S,{ordinal},0,0,0,0,0,0,0");
            Assert.True(status is not null, $"ordinal {ordinal} did not parse");
            Assert.Equal(expected, status!.State);
        }
    }

    /// <summary>
    /// The firmware's own idle line, taken from the Serial Monitor during
    /// bring-up. Guards the field order, which is the easiest thing to get
    /// wrong when porting.
    /// </summary>
    [Fact]
    public void The_idle_line_seen_on_hardware_decodes_as_idle()
    {
        var status = StatusParser.Parse("S,1,0,0,0,0,3,499000,1");

        Assert.NotNull(status);
        Assert.Equal(SocketState.Ready, status!.State);
        Assert.Equal(0, status.CurrentMa);
        Assert.Equal(0, status.SessionElapsedMs);
        Assert.Equal(3, status.CutoffCount);
        Assert.Equal(499_000L, status.TotalSavedMs);
        Assert.True(status.RelayClosed);
        Assert.True(status.State.IsPowerOn());
    }
}
