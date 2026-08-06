namespace SmartSocket.Core;

/// <summary>
/// Mirrors <c>SocketState</c> in <c>SmartSocket/src/core/Types.h</c>, and
/// <c>SocketState</c> in the Android app.
///
/// The ordinals are the wire format - the firmware sends the number, not the
/// name, because a 32 KB part has better uses for its flash than strings the
/// client already has. <b>If the enum in Types.h gains a member anywhere but
/// the end, this list has to move with it.</b>
/// </summary>
public enum SocketState
{
    Calibrating = 0,
    Ready = 1,
    Settling = 2,
    Charging = 3,
    Cutoff = 4,
    Fault = 5,
    ManualOff = 6,
    Probing = 7,
    RelayStuck = 8,
}

public static class SocketStateExtensions
{
    /// <summary>Highest ordinal the firmware can send.</summary>
    public const int MaxOrdinal = (int)SocketState.RelayStuck;

    public static string Label(this SocketState state) => state switch
    {
        SocketState.Calibrating => "Calibrating",
        SocketState.Ready => "Ready",
        SocketState.Settling => "Settling",
        SocketState.Charging => "Charging",
        SocketState.Cutoff => "Power cut",
        SocketState.Fault => "Overcurrent",
        SocketState.ManualOff => "Switched off",
        SocketState.Probing => "Checking",
        SocketState.RelayStuck => "Relay stuck",
        _ => "Unknown",
    };

    public static string Detail(this SocketState state) => state switch
    {
        SocketState.Calibrating => "Measuring the sensor's zero point",
        SocketState.Ready => "Waiting for something to be plugged in",
        SocketState.Settling => "Ignoring the charger's inrush",
        SocketState.Charging => "Watching for the current to taper",
        SocketState.Cutoff => "Charged. Power is off",
        SocketState.Fault => "Something is wrong. Unplug at the wall",
        SocketState.ManualOff => "You turned it off",
        SocketState.Probing => "Looking for a load",
        SocketState.RelayStuck => "Power is still flowing. Unplug at the wall",
        _ => string.Empty,
    };

    public static bool IsPowerOn(this SocketState state) =>
        state is SocketState.Ready
            or SocketState.Settling
            or SocketState.Charging
            or SocketState.Probing;

    public static bool IsAlarm(this SocketState state) =>
        state is SocketState.Fault or SocketState.RelayStuck;

    /// <summary>
    /// Null for an ordinal this build does not know. An unknown ordinal means
    /// the firmware and this app have drifted apart, and inventing a state for
    /// it would hide exactly that.
    /// </summary>
    public static SocketState? FromOrdinal(int value) =>
        value >= 0 && value <= MaxOrdinal ? (SocketState)value : null;
}

/// <summary>One decoded status line.</summary>
public sealed record SocketStatus(
    SocketState State,
    int CurrentMa,
    int PeakMa,
    int ThresholdMa,
    long SessionElapsedMs,
    int CutoffCount,
    long TotalSavedMs,
    bool RelayClosed)
{
    public float Amps => CurrentMa / 1000f;
    public float PeakAmps => PeakMa / 1000f;
    public float ThresholdAmps => ThresholdMa / 1000f;

    public static readonly SocketStatus Unknown = new(
        State: SocketState.Calibrating,
        CurrentMa: 0,
        PeakMa: 0,
        ThresholdMa: 0,
        SessionElapsedMs: 0,
        CutoffCount: 0,
        TotalSavedMs: 0,
        RelayClosed: false);
}

/// <summary>
/// Commands the socket accepts. One byte each - see <c>HalTelemetry::pump</c>,
/// which only acts on a line that is exactly one character long.
/// </summary>
public enum SocketCommand
{
    Cut,
    Rearm,
    Probe,
    StatusNow,

    /// <summary>
    /// Zero the socket's lifetime cutoff count and saved-time total. They live
    /// in the socket's EEPROM, not in the app, so clearing the app's history
    /// left them untouched and looking stuck.
    /// </summary>
    ResetTotals,
}

public static class SocketCommandExtensions
{
    public static string Wire(this SocketCommand command) => command switch
    {
        SocketCommand.Cut => "C",
        SocketCommand.Rearm => "R",
        SocketCommand.Probe => "P",
        SocketCommand.StatusNow => "?",
        SocketCommand.ResetTotals => "Z",
        _ => throw new ArgumentOutOfRangeException(nameof(command)),
    };
}
