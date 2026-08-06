using SmartSocket.Core;

namespace SmartSocket.App.Transport;

/// <summary>
/// How the bytes get there. Three, not one, because "Bluetooth" is two
/// incompatible radios sharing a name.
/// </summary>
public enum LinkKind
{
    /// <summary>A COM port: the USB cable, or a Classic HC-05's SPP port.</summary>
    Serial,

    /// <summary>
    /// Bluetooth Low Energy. A different radio and a different protocol from
    /// Classic - no RFCOMM, no serial port, no COM number. Windows never
    /// assigns one, which is exactly why a BLE module looks absent from a port
    /// list however long you stare at it.
    /// </summary>
    Ble,

    /// <summary>The socket's state machine in software.</summary>
    Demo,
}

/// <summary>
/// A socket you can open.
///
/// <see cref="Id"/> means whatever the transport needs: a COM port name for
/// <see cref="LinkKind.Serial"/>, a Windows device-interface id for
/// <see cref="LinkKind.Ble"/>.
/// </summary>
public sealed record SocketDevice(string Name, string Id, LinkKind Kind)
{
    public static readonly SocketDevice Demo = new("Demo socket", "DEMO", LinkKind.Demo);

    public bool IsDemo => Kind == LinkKind.Demo;

    /// <summary>How the device is shown in a list, radio and all.</summary>
    public string Display => Kind switch
    {
        LinkKind.Serial => Name,
        LinkKind.Ble => $"{Name}  (Bluetooth LE)",
        _ => Name,
    };
}

public abstract record LinkState
{
    public sealed record Idle : LinkState;

    public sealed record Connecting : LinkState;

    public sealed record Connected(SocketDevice Device) : LinkState;

    /// <summary>
    /// The link dropped on its own and is being rebuilt. Distinct from
    /// <see cref="Connecting"/>, which is a link the user just asked for: a
    /// reconnect must not throw the user back to the device picker.
    /// </summary>
    public sealed record Reconnecting(SocketDevice Device, int Attempt) : LinkState;

    public sealed record Failed(string Reason) : LinkState;
}

/// <summary>
/// The link to a socket.
///
/// An interface with several implementations for the same reason the firmware
/// has <c>ICurrentSensor</c>: the UI must be buildable and demonstrable with no
/// hardware in the room, and the same screens must work over a cable, over
/// Classic Bluetooth and over BLE without knowing which they are on.
/// </summary>
public interface ISocketTransport
{
    event Action<LinkState>? LinkStateChanged;

    event Action<SocketStatus>? StatusChanged;

    /// <summary>
    /// Opens the link and waits for the socket to prove it is one. Returns
    /// whether that succeeded, so a caller retrying on a schedule does not have
    /// to race the event to find out.
    /// </summary>
    Task<bool> ConnectAsync(SocketDevice device, CancellationToken ct = default);

    Task SendAsync(SocketCommand command);

    /// <summary>
    /// One raw line, newline appended by the transport.
    ///
    /// For the commands that carry a value and so cannot be a
    /// <see cref="SocketCommand"/>: <c>A1</c>/<c>A0</c> to take and hand back
    /// the full-charge decision, and <c>B&lt;percent&gt;</c> to report this
    /// machine's battery for the socket's display.
    /// </summary>
    Task SendLineAsync(string line);

    void Disconnect();
}
