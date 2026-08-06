using System.Text;
using SmartSocket.Core;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;

namespace SmartSocket.App.Transport;

/// <summary>
/// The link to a Bluetooth Low Energy module.
///
/// WHY THIS EXISTS. "Bluetooth" is two incompatible radios. Classic (BR/EDR)
/// has SPP, which Windows exposes as a COM port, and that is what the HC-05 was
/// assumed to be. BLE has no SPP and no COM port at all - every vendor bolted a
/// serial channel onto GATT with private UUIDs. A BLE module can therefore be
/// paired, working, and completely invisible to a port list, which is precisely
/// what this project ran into: Windows reported a "Bluetooth LE Generic
/// Attribute Service" while every COM port on the machine belonged to something
/// else.
///
/// Which characteristics carry the bytes is worked out by
/// <see cref="BleSerialProfiles"/>, in the testable core, so this class only has
/// to deal with the radio.
/// </summary>
public sealed class BleTransport : ISocketTransport
{
    /// <summary>
    /// Long enough for a module to finish GATT discovery and answer, short
    /// enough that a wrong device is reported rather than hung on.
    /// </summary>
    private static readonly TimeSpan HandshakeTimeout = TimeSpan.FromSeconds(10);

    /// <summary>
    /// How many times to re-enumerate before believing a device really has no
    /// serial service. Three covers the settling window without making a
    /// genuinely wrong device take a minute to reject.
    /// </summary>
    private const int DiscoveryAttempts = 3;

    private BluetoothLEDevice? _device;
    private GattCharacteristic? _notify;
    private GattCharacteristic? _write;

    /// <summary>
    /// Held open deliberately. Without MaintainConnection, Windows drops a BLE
    /// link the moment it thinks nothing needs it - which is why the socket
    /// would connect, work, and then be gone a minute later for no visible
    /// reason.
    /// </summary>
    private GattSession? _session;

    /// <summary>
    /// THESE MUST BE KEPT ALIVE. GattDeviceService is IDisposable, and when the
    /// last reference to one is collected Windows tears down the GATT session
    /// underneath it - taking the characteristics and the connection with it.
    /// Holding only the characteristics, as this class first did, means the link
    /// dies at whatever arbitrary moment the garbage collector next runs.
    /// </summary>
    private readonly List<GattDeviceService> _services = [];

    private readonly StringBuilder _line = new();
    private readonly object _gate = new();

    private volatile bool _sawBytes;
    private volatile bool _sawStatus;
    private volatile bool _closing;

    public event Action<LinkState>? LinkStateChanged;
    public event Action<SocketStatus>? StatusChanged;

    /// <summary>Which profile matched, for the error messages.</summary>
    public string? ProfileName { get; private set; }

    /// <summary>
    /// Every BLE device Windows has paired. Deliberately paired-only: scanning
    /// would need a live radio sweep and would list every fitness tracker in
    /// the building, and the socket has to be paired to be useful anyway.
    /// </summary>
    public static async Task<IReadOnlyList<SocketDevice>> PairedDevicesAsync()
    {
        try
        {
            var selector = BluetoothLEDevice.GetDeviceSelectorFromPairingState(true);
            var found = await DeviceInformation.FindAllAsync(selector);

            return found
                .Select(d => new SocketDevice(
                    string.IsNullOrWhiteSpace(d.Name) ? "Unnamed BLE device" : d.Name,
                    d.Id,
                    LinkKind.Ble))
                .ToList();
        }
        catch (Exception)
        {
            // No radio, or the adapter is off. An empty list is the honest
            // answer and the caller shows only the COM ports.
            return [];
        }
    }

    public async Task<bool> ConnectAsync(SocketDevice device, CancellationToken ct = default)
    {
        Disconnect();
        _closing = false;
        LinkStateChanged?.Invoke(new LinkState.Connecting());

        _sawBytes = false;
        _sawStatus = false;
        lock (_gate) _line.Clear();

        BluetoothLEDevice? le;
        try
        {
            le = await BluetoothLEDevice.FromIdAsync(device.Id);
        }
        catch (Exception e)
        {
            Fail($"Could not open {device.Name}. {e.Message}");
            return false;
        }

        if (le is null)
        {
            Fail($"Windows could not open {device.Name}. Re-pair it in Bluetooth settings.");
            return false;
        }

        _device = le;
        le.ConnectionStatusChanged += OnConnectionStatusChanged;

        // Ask Windows to hold the radio link up. Without this the connection is
        // dropped as soon as Windows decides nothing needs it, which looks from
        // the outside like the socket randomly disappearing.
        try
        {
            _session = await GattSession.FromDeviceIdAsync(le.BluetoothDeviceId);
            _session.MaintainConnection = true;
        }
        catch (Exception)
        {
            // Not fatal - the link still works, it is just easier to lose.
        }

        // Discovery is retried because the first attempt frequently comes back
        // with only 0x1800 and 0x1801, the two services every BLE device must
        // have. That is not a device without a serial channel; it is a device
        // whose vendor service has not been enumerated yet, usually because the
        // radio connected a moment ago and GATT is still settling.
        List<BleService> described = [];
        Dictionary<Guid, List<GattCharacteristic>> handles = [];
        BleSerialProfile? profile = null;

        for (var attempt = 0; attempt < DiscoveryAttempts && profile is null; attempt++)
        {
            if (attempt > 0) await Task.Delay(1200, CancellationToken.None).ConfigureAwait(false);
            if (ct.IsCancellationRequested) break;

            described = [];
            handles = [];
            DisposeServices();

            // Uncached: a stale service list is the classic way this fails
            // silently after a module has been power-cycled or reflashed.
            var services = await le.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            if (services.Status != GattCommunicationStatus.Success) continue;

            foreach (var service in services.Services)
            {
                _services.Add(service);

                var chars = await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
                if (chars.Status != GattCommunicationStatus.Success) continue;

                var describedCharacteristics = new List<BleCharacteristic>();
                var live = new List<GattCharacteristic>();

                foreach (var c in chars.Characteristics)
                {
                    var p = c.CharacteristicProperties;

                    describedCharacteristics.Add(new BleCharacteristic(
                        c.Uuid,
                        CanNotify: p.HasFlag(GattCharacteristicProperties.Notify)
                                   || p.HasFlag(GattCharacteristicProperties.Indicate),
                        CanWrite: p.HasFlag(GattCharacteristicProperties.Write)
                                  || p.HasFlag(GattCharacteristicProperties.WriteWithoutResponse)));

                    live.Add(c);
                }

                described.Add(new BleService(service.Uuid, describedCharacteristics));
                handles[service.Uuid] = live;
            }

            profile = BleSerialProfiles.Resolve(described);
        }

        if (profile is null)
        {
            // Naming what was actually found, rather than only what was wanted.
            // A module nobody has catalogued is exactly the case where the
            // service list is the difference between "buy another one" and
            // "add four lines to the profile table".
            var seen = string.Join(", ", described.Select(s => s.Uuid.ToString("D")));

            var onlyMandatory = described.Count > 0 && described.All(s =>
                s.Uuid.ToString("D").StartsWith("00001800-", StringComparison.OrdinalIgnoreCase) ||
                s.Uuid.ToString("D").StartsWith("00001801-", StringComparison.OrdinalIgnoreCase));

            Fail(onlyMandatory
                ? $"{device.Name} reported only the two services every BLE device has, and none of its own - after {DiscoveryAttempts} attempts. Power-cycle the socket, wait for the LED to blink fast, and try again."
                : $"{device.Name} is a Bluetooth LE device with no serial channel - nothing on it can carry the socket's data. Services found: {(seen.Length == 0 ? "none" : seen)}");

            Disconnect();
            return false;
        }

        ProfileName = profile.Name;

        if (!handles.TryGetValue(profile.Service, out var candidates))
        {
            Fail($"{device.Name} lost its serial service between discovery and use.");
            Disconnect();
            return false;
        }

        _notify = candidates.FirstOrDefault(c => c.Uuid == profile.Notify);
        _write = candidates.FirstOrDefault(c => c.Uuid == profile.Write);

        if (_notify is null || _write is null)
        {
            Fail($"{device.Name} did not offer the characteristics it advertised.");
            Disconnect();
            return false;
        }

        // Subscribing is what makes the module start pushing bytes up. Without
        // this the link is open and permanently silent.
        _notify.ValueChanged += OnValueChanged;

        var wanted = _notify.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Notify)
            ? GattClientCharacteristicConfigurationDescriptorValue.Notify
            : GattClientCharacteristicConfigurationDescriptorValue.Indicate;

        GattCommunicationStatus subscribed;
        try
        {
            subscribed = await _notify.WriteClientCharacteristicConfigurationDescriptorAsync(wanted);
        }
        catch (Exception e)
        {
            Fail($"Could not subscribe to {device.Name}. {e.Message}");
            Disconnect();
            return false;
        }

        if (subscribed != GattCommunicationStatus.Success)
        {
            Fail($"{device.Name} refused a subscription ({subscribed}).");
            Disconnect();
            return false;
        }

        await SendAsync(SocketCommand.StatusNow).ConfigureAwait(false);

        var deadline = DateTime.UtcNow + HandshakeTimeout;
        while (DateTime.UtcNow < deadline)
        {
            if (_sawStatus)
            {
                LinkStateChanged?.Invoke(new LinkState.Connected(device));
                return true;
            }

            if (ct.IsCancellationRequested) break;
            await Task.Delay(100, CancellationToken.None).ConfigureAwait(false);
        }

        var reason = _sawBytes
            ? $"{device.Name} is sending data this app cannot read. If the module has a serial baud rate setting, the firmware uses 9600."
            : $"{device.Name} connected but never sent anything. Check the socket is powered, and that the module's TXD reaches Arduino pin 0.";

        Disconnect();
        Fail(reason);
        return false;
    }

    public Task SendAsync(SocketCommand command) => SendLineAsync(command.Wire());

    public async Task SendLineAsync(string line)
    {
        var write = _write;
        if (write is null) return;

        try
        {
            var buffer = CryptographicBuffer.ConvertStringToBinary(
                line + "\n",
                BinaryStringEncoding.Utf8);

            // WriteWithoutResponse where the module allows it: these modules are
            // slow to acknowledge, and a two-byte command does not need one.
            var mode = write.CharacteristicProperties
                .HasFlag(GattCharacteristicProperties.WriteWithoutResponse)
                ? GattWriteOption.WriteWithoutResponse
                : GattWriteOption.WriteWithResponse;

            await write.WriteValueAsync(buffer, mode);
        }
        catch (Exception)
        {
            // A write to a device that has just gone out of range throws. The
            // connection-status handler reports that once.
        }
    }

    public void Disconnect()
    {
        _closing = true;

        if (_notify is not null)
        {
            _notify.ValueChanged -= OnValueChanged;
            _notify = null;
        }

        _write = null;

        // Order matters on the way out too: services first, then the session,
        // then the device. Disposing the device while services are still open
        // leaves Windows holding a connection nothing can close.
        DisposeServices();

        if (_session is not null)
        {
            try { _session.MaintainConnection = false; } catch (Exception) { /* already gone */ }
            try { _session.Dispose(); } catch (Exception) { /* already gone */ }
            _session = null;
        }

        var device = _device;
        _device = null;

        if (device is not null)
        {
            device.ConnectionStatusChanged -= OnConnectionStatusChanged;
            try { device.Dispose(); } catch (Exception) { /* already gone */ }
        }

        lock (_gate) _line.Clear();
    }

    private void DisposeServices()
    {
        foreach (var service in _services)
        {
            try { service.Dispose(); } catch (Exception) { /* already gone */ }
        }

        _services.Clear();
    }

    private void OnConnectionStatusChanged(BluetoothLEDevice sender, object args)
    {
        if (_closing) return;
        if (sender.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
        {
            LinkStateChanged?.Invoke(new LinkState.Failed("The link to the socket dropped."));
        }
    }

    private void OnValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        CryptographicBuffer.CopyToByteArray(args.CharacteristicValue, out var bytes);
        if (bytes is null || bytes.Length == 0) return;

        _sawBytes = true;

        foreach (var b in bytes)
        {
            var c = (char)b;

            if (c == '\r') continue;

            if (c != '\n')
            {
                lock (_gate)
                {
                    // BLE delivers 20-byte packets, so a status line always
                    // arrives in pieces and has to be reassembled. Capping the
                    // buffer stops a module that never sends a newline from
                    // growing it without bound.
                    if (_line.Length < 128) _line.Append(c);
                }
                continue;
            }

            string complete;
            lock (_gate)
            {
                complete = _line.ToString();
                _line.Clear();
            }

            if (StatusParser.Parse(complete) is { } status)
            {
                _sawStatus = true;
                StatusChanged?.Invoke(status);
            }
        }
    }

    private void Fail(string reason) => LinkStateChanged?.Invoke(new LinkState.Failed(reason));
}
