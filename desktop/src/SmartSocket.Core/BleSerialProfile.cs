namespace SmartSocket.Core;

/// <summary>What a discovered GATT characteristic can do.</summary>
public sealed record BleCharacteristic(
    Guid Uuid,
    bool CanNotify,
    bool CanWrite);

public sealed record BleService(Guid Uuid, IReadOnlyList<BleCharacteristic> Characteristics);

/// <summary>
/// Which characteristic carries bytes each way on a BLE serial module.
/// <see cref="Notify"/> is device to host, <see cref="Write"/> is host to
/// device. They are frequently the same characteristic.
/// </summary>
public sealed record BleSerialProfile(Guid Service, Guid Notify, Guid Write, string Name);

/// <summary>
/// Works out how to speak serial over a BLE module that has no standard for it.
///
/// BLE has no equivalent of Classic's SPP. Every vendor bolted a serial channel
/// onto GATT with private UUIDs, so a module advertising itself as "HC-05" may
/// be any of half a dozen designs - or something nobody has catalogued. Rather
/// than hardcode one vendor and fail on the rest, this tries the known ones by
/// name and then falls back to what the device says it can do.
///
/// Pure logic with no Bluetooth stack in it, so the matching can be tested
/// against a table of real modules instead of by carrying boards around. Both
/// the Android app and the Windows app run this same resolution, so a module
/// that works on one works on the other.
/// </summary>
public static class BleSerialProfiles
{
    private static Guid Short(ushort id) =>
        new($"0000{id:X4}-0000-1000-8000-00805F9B34FB");

    /// <summary>
    /// The catalogued designs, most common first. HM-10 and its clones - AT-09,
    /// JDY-08, CC2541 boards, and most things sold as a "BLE HC-05" - are by a
    /// wide margin the likeliest, so they are tried first.
    /// </summary>
    public static readonly IReadOnlyList<BleSerialProfile> Known =
    [
        // HM-10 / AT-09 / JDY / CC2541. One characteristic, both directions.
        new(Short(0xFFE0), Short(0xFFE1), Short(0xFFE1), "HM-10 / FFE0"),

        // Nordic UART Service. Note the crossover: 0003 is the device's TX, so
        // it is the host's notify, and 0002 is what the host writes.
        new(
            new Guid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"),
            new Guid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"),
            new Guid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"),
            "Nordic UART"),

        // Microchip RN4870 / RN4871 transparent UART.
        new(
            new Guid("49535343-FE7D-4AE5-8FA9-9FAFD205E455"),
            new Guid("49535343-1E4D-4BD9-BA61-23C647249616"),
            new Guid("49535343-8841-43F4-A8D4-ECBE34729BB3"),
            "Microchip transparent UART"),

        // Feasycom and several other FFF0-range modules, split characteristics.
        new(Short(0xFFF0), Short(0xFFF1), Short(0xFFF2), "FFF0 split"),
    ];

    /// <summary>
    /// Services every BLE device carries. None of them is a serial channel, and
    /// the fallback must not pick a characteristic out of one - writing to
    /// Device Name because it happened to be writable would be worse than
    /// failing.
    /// </summary>
    private static readonly HashSet<Guid> Standard =
    [
        Short(0x1800), // Generic Access
        Short(0x1801), // Generic Attribute
        Short(0x180A), // Device Information
        Short(0x180F), // Battery
        Short(0x1805), // Current Time
        Short(0x181C), // User Data
    ];

    /// <summary>
    /// Null when nothing on the device can carry a byte stream, which is a real
    /// answer: some BLE devices genuinely have no serial channel, and guessing
    /// one would produce a link that connects and never works.
    /// </summary>
    public static BleSerialProfile? Resolve(IEnumerable<BleService> services)
    {
        var list = services as IReadOnlyList<BleService> ?? services.ToList();

        // 1. A catalogued design, matched exactly.
        foreach (var known in Known)
        {
            var service = list.FirstOrDefault(s => s.Uuid == known.Service);
            if (service is null) continue;

            var notify = service.Characteristics.FirstOrDefault(c => c.Uuid == known.Notify && c.CanNotify);
            var write = service.Characteristics.FirstOrDefault(c => c.Uuid == known.Write && c.CanWrite);

            if (notify is not null && write is not null) return known;
        }

        // 2. Anything that behaves like one. A serial channel needs a way for
        //    the device to push bytes up and for the host to push bytes down;
        //    if a vendor service offers both, it is almost certainly the UART.
        foreach (var service in list)
        {
            if (Standard.Contains(service.Uuid)) continue;

            var notify = service.Characteristics.FirstOrDefault(c => c.CanNotify);
            var write = service.Characteristics.FirstOrDefault(c => c.CanWrite);

            if (notify is not null && write is not null)
            {
                return new BleSerialProfile(
                    service.Uuid,
                    notify.Uuid,
                    write.Uuid,
                    "Discovered");
            }
        }

        return null;
    }
}
