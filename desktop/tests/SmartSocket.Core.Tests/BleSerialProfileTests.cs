using SmartSocket.Core;
using Xunit;

namespace SmartSocket.Core.Tests;

/// <summary>
/// Matching real modules, since the alternative is carrying boards around.
/// Every service layout below is one an actual BLE serial module presents.
/// </summary>
public class BleSerialProfileTests
{
    private static Guid Short(ushort id) => new($"0000{id:X4}-0000-1000-8000-00805F9B34FB");

    private static BleService Generic() => new(
        Short(0x1800),
        [new BleCharacteristic(Short(0x2A00), CanNotify: false, CanWrite: true)]);

    private static BleService DeviceInfo() => new(
        Short(0x180A),
        [new BleCharacteristic(Short(0x2A29), CanNotify: false, CanWrite: false)]);

    [Fact]
    public void An_HM10_is_matched_on_its_single_dual_purpose_characteristic()
    {
        var services = new List<BleService>
        {
            Generic(),
            new(Short(0xFFE0), [new BleCharacteristic(Short(0xFFE1), CanNotify: true, CanWrite: true)]),
        };

        var profile = BleSerialProfiles.Resolve(services);

        Assert.NotNull(profile);
        Assert.Equal("HM-10 / FFE0", profile!.Name);
        Assert.Equal(Short(0xFFE1), profile.Notify);
        Assert.Equal(Short(0xFFE1), profile.Write);
    }

    /// <summary>
    /// The crossover is the easy thing to get backwards: 0003 is the *device's*
    /// TX, so it is what the host subscribes to. Writing to it instead would
    /// connect cleanly and never deliver a command.
    /// </summary>
    [Fact]
    public void Nordic_UART_directions_are_not_swapped()
    {
        var nus = new Guid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
        var tx = new Guid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");
        var rx = new Guid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");

        var profile = BleSerialProfiles.Resolve([
            new BleService(nus, [
                new BleCharacteristic(tx, CanNotify: true, CanWrite: false),
                new BleCharacteristic(rx, CanNotify: false, CanWrite: true),
            ]),
        ]);

        Assert.NotNull(profile);
        Assert.Equal(tx, profile!.Notify);
        Assert.Equal(rx, profile.Write);
    }

    [Fact]
    public void An_uncatalogued_module_is_resolved_from_what_it_can_do()
    {
        var vendor = new Guid("12345678-1234-5678-1234-56789ABCDEF0");
        var up = new Guid("12345678-1234-5678-1234-56789ABCDEF1");
        var down = new Guid("12345678-1234-5678-1234-56789ABCDEF2");

        var profile = BleSerialProfiles.Resolve([
            Generic(),
            DeviceInfo(),
            new BleService(vendor, [
                new BleCharacteristic(up, CanNotify: true, CanWrite: false),
                new BleCharacteristic(down, CanNotify: false, CanWrite: true),
            ]),
        ]);

        Assert.NotNull(profile);
        Assert.Equal("Discovered", profile!.Name);
        Assert.Equal(vendor, profile.Service);
        Assert.Equal(up, profile.Notify);
        Assert.Equal(down, profile.Write);
    }

    /// <summary>
    /// Device Name in Generic Access is writable on plenty of modules. Picking
    /// it as a serial channel would rename the device on every command sent.
    /// </summary>
    [Fact]
    public void Standard_services_are_never_used_as_a_serial_channel()
    {
        var profile = BleSerialProfiles.Resolve([
            new BleService(Short(0x1800), [
                new BleCharacteristic(Short(0x2A00), CanNotify: true, CanWrite: true),
            ]),
            new BleService(Short(0x180F), [
                new BleCharacteristic(Short(0x2A19), CanNotify: true, CanWrite: true),
            ]),
        ]);

        Assert.Null(profile);
    }

    [Fact]
    public void A_device_with_no_serial_channel_reports_none_rather_than_guessing()
    {
        Assert.Null(BleSerialProfiles.Resolve([Generic(), DeviceInfo()]));
        Assert.Null(BleSerialProfiles.Resolve([]));
    }

    /// <summary>
    /// A characteristic that can be notified but not written is half a link.
    /// Accepting it would give a connection that shows live data and silently
    /// drops every Cut command - the worst possible failure for this app.
    /// </summary>
    [Fact]
    public void Notify_without_write_is_not_a_serial_channel()
    {
        var vendor = new Guid("12345678-1234-5678-1234-56789ABCDEF0");

        var profile = BleSerialProfiles.Resolve([
            new BleService(vendor, [
                new BleCharacteristic(Guid.NewGuid(), CanNotify: true, CanWrite: false),
            ]),
        ]);

        Assert.Null(profile);
    }

    [Fact]
    public void Write_without_notify_is_not_a_serial_channel()
    {
        var vendor = new Guid("12345678-1234-5678-1234-56789ABCDEF0");

        var profile = BleSerialProfiles.Resolve([
            new BleService(vendor, [
                new BleCharacteristic(Guid.NewGuid(), CanNotify: false, CanWrite: true),
            ]),
        ]);

        Assert.Null(profile);
    }

    /// <summary>
    /// A module carrying both a catalogued service and some other vendor
    /// service must use the catalogued one - the fallback is a guess and the
    /// table is not.
    /// </summary>
    [Fact]
    public void A_known_profile_wins_over_the_fallback()
    {
        var vendor = new Guid("12345678-1234-5678-1234-56789ABCDEF0");

        var profile = BleSerialProfiles.Resolve([
            new BleService(vendor, [
                new BleCharacteristic(Guid.NewGuid(), CanNotify: true, CanWrite: true),
            ]),
            new BleService(Short(0xFFE0), [
                new BleCharacteristic(Short(0xFFE1), CanNotify: true, CanWrite: true),
            ]),
        ]);

        Assert.Equal("HM-10 / FFE0", profile!.Name);
    }

    /// <summary>
    /// FFE0 present but its characteristic missing the properties the table
    /// expects: the exact match fails, and the fallback should still rescue it
    /// rather than the whole resolution collapsing.
    /// </summary>
    [Fact]
    public void A_partial_known_service_falls_through_to_discovery()
    {
        var other = new Guid("0000AB00-0000-1000-8000-00805F9B34FB");

        var profile = BleSerialProfiles.Resolve([
            new BleService(Short(0xFFE0), [
                new BleCharacteristic(Short(0xFFE1), CanNotify: true, CanWrite: false),
            ]),
            new BleService(other, [
                new BleCharacteristic(Guid.NewGuid(), CanNotify: true, CanWrite: true),
            ]),
        ]);

        Assert.NotNull(profile);
        Assert.Equal("Discovered", profile!.Name);
        Assert.Equal(other, profile.Service);
    }
}
