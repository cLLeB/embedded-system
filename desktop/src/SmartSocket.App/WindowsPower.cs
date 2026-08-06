using System.Runtime.InteropServices;

namespace SmartSocket.App;

/// <summary>
/// The laptop's own battery, straight from Win32.
///
/// This is the desktop half of the feature the current sensor cannot provide.
/// The socket can see a laptop charging - 150 mA is well clear of the ACS712's
/// noise - so it will cut on its own when the current tapers, at whatever
/// percentage that happens to be, typically the high nineties. That is late if
/// what you want is to stop at 80% for the sake of the battery's lifespan.
/// Windows knows the exact percentage, so the app can ask for the cut at the
/// number the user actually chose.
///
/// GetSystemPowerStatus rather than a WMI query: it is a single non-blocking
/// call into kernel32 with no COM apartment, no WMI service dependency and no
/// several-hundred-millisecond first call. Win32_Battery would give more detail
/// than is needed here and cost all three.
/// </summary>
internal static class WindowsPower
{
    [StructLayout(LayoutKind.Sequential)]
    private struct SystemPowerStatus
    {
        public byte ACLineStatus;
        public byte BatteryFlag;
        public byte BatteryLifePercent;
        public byte SystemStatusFlag;
        public int BatteryLifeTime;
        public int BatteryFullLifeTime;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetSystemPowerStatus(out SystemPowerStatus status);

    /// <summary>Windows reports 255 when it does not know.</summary>
    private const byte PercentUnknown = 255;

    private const byte AcOffline = 0;
    private const byte AcOnline = 1;

    /// <summary>No battery in the machine - a desktop PC.</summary>
    private const byte NoSystemBattery = 128;

    public readonly record struct PowerState(
        int Percent,
        bool OnAcPower,
        bool HasBattery);

    /// <summary>
    /// Null if Win32 refused the call, which is different from "no battery" and
    /// must not be read as 0%.
    /// </summary>
    public static PowerState? Read()
    {
        if (!GetSystemPowerStatus(out var status)) return null;

        var hasBattery = (status.BatteryFlag & NoSystemBattery) == 0;

        var percent = status.BatteryLifePercent == PercentUnknown
            ? -1
            : status.BatteryLifePercent;

        var onAc = status.ACLineStatus switch
        {
            AcOnline => true,
            AcOffline => false,
            // 255 means unknown. Treating unknown as "on AC" would let the app
            // cut power to a machine it cannot tell is plugged in; treating it
            // as off battery-power simply means no cut, which is the safe way
            // to be wrong.
            _ => false,
        };

        return new PowerState(percent, onAc, hasBattery);
    }
}
