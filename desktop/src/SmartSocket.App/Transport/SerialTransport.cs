using System.IO.Ports;
using System.Text;
using SmartSocket.Core;

namespace SmartSocket.App.Transport;

/// <summary>
/// The real link: a COM port at 9600 8N1.
///
/// This covers both routes to the socket without knowing which one it is on.
/// The Uno's USB cable enumerates as a COM port through its CDC/CH340 driver,
/// and Windows exposes a paired HC-05's SPP profile as an outgoing virtual COM
/// port. The firmware speaks the same <c>config::TelemetryBaud</c> either way,
/// because the HC-05 sits on the same hardware UART the USB port uses.
///
/// AN OPEN PORT IS NOT A SMART SOCKET. Windows will happily open a COM port
/// belonging to a printer, a GPS dongle or a virtual null-modem. So connecting
/// is not finished until the thing on the other end has answered a "?" with a
/// line this app can parse, and the three ways that can fail are reported
/// separately because they have different fixes.
/// </summary>
public sealed class SerialTransport : ISocketTransport
{
    private const int Baud = 9600;

    /// <summary>
    /// Long enough for a reset. Opening a COM port on an Uno pulls DTR, which
    /// reboots the board; it then spends time in the bootloader before the
    /// sketch runs and the first status line appears a second later.
    /// </summary>
    private static readonly TimeSpan HandshakeTimeout = TimeSpan.FromSeconds(6);

    private SerialPort? _port;
    private CancellationTokenSource? _readerCts;
    private Task? _reader;

    private readonly StringBuilder _line = new();
    private readonly object _gate = new();

    /// <summary>
    /// Two separate facts, and the difference between them is the whole
    /// diagnosis. Bytes with no status line means the port is fine and the two
    /// ends disagree about the baud rate; no bytes at all means nothing on that
    /// port is talking, which is a cable or the wrong port.
    /// </summary>
    private volatile bool _sawBytes;
    private volatile bool _sawStatus;

    public event Action<LinkState>? LinkStateChanged;
    public event Action<SocketStatus>? StatusChanged;

    /// <summary>Every COM port Windows currently knows about.</summary>
    public static IReadOnlyList<SocketDevice> AvailablePorts()
    {
        try
        {
            return SerialPort.GetPortNames()
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(p => p, StringComparer.OrdinalIgnoreCase)
                .Select(p => new SocketDevice(p, p, LinkKind.Serial))
                .ToList();
        }
        catch (Exception)
        {
            // Enumeration can throw if a driver is mid-install. An empty list is
            // the honest answer and the caller shows "no ports".
            return [];
        }
    }

    public async Task<bool> ConnectAsync(SocketDevice device, CancellationToken ct = default)
    {
        Disconnect();
        LinkStateChanged?.Invoke(new LinkState.Connecting());

        _sawBytes = false;
        _sawStatus = false;
        lock (_gate) _line.Clear();

        SerialPort port;
        try
        {
            port = new SerialPort(device.Id, Baud, Parity.None, 8, StopBits.One)
            {
                ReadTimeout = 500,
                WriteTimeout = 1000,
                NewLine = "\n",
                // The Uno resets when DTR is asserted. That is wanted - it puts
                // the firmware in a known state - but it is why the handshake
                // has to tolerate several seconds of silence first.
                DtrEnable = true,
                RtsEnable = true,
            };
            port.Open();
        }
        catch (UnauthorizedAccessException)
        {
            Fail($"{device.Id} is already open in another program. Close the Arduino IDE's Serial Monitor and try again.");
            return false;
        }
        catch (Exception e)
        {
            Fail($"Could not open {device.Id}. {e.Message}");
            return false;
        }

        _port = port;
        _readerCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        _reader = Task.Run(() => ReadLoop(port, _readerCts.Token), CancellationToken.None);

        // Ask, rather than wait. The socket publishes once a second on its own,
        // but "?" makes it answer immediately, so a good link proves itself fast.
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

        // Same three outcomes the Android app reports, for the same reason:
        // each one has a different fix, and "it didn't work" has none.
        var reason = _sawBytes
            ? $"{device.Id} is sending data this app cannot read. Check the baud rate - the firmware uses {Baud}."
            : $"{device.Id} opened but never sent anything. Check it is the right port, and that the socket is powered.";

        Disconnect();
        Fail(reason);
        return false;
    }

    public Task SendAsync(SocketCommand command) => SendLineAsync(command.Wire());

    public Task SendLineAsync(string line)
    {
        var port = _port;
        if (port is null || !port.IsOpen) return Task.CompletedTask;

        try
        {
            port.Write(line + "\n");
        }
        catch (Exception)
        {
            // A write to a port that has just been unplugged throws. The read
            // loop notices the same failure and reports it once.
        }

        return Task.CompletedTask;
    }

    public void Disconnect()
    {
        try { _readerCts?.Cancel(); } catch (Exception) { /* already gone */ }

        var port = _port;
        _port = null;

        if (port is not null)
        {
            try { if (port.IsOpen) port.Close(); } catch (Exception) { /* already closed */ }
            try { port.Dispose(); } catch (Exception) { /* already disposed */ }
        }

        _readerCts?.Dispose();
        _readerCts = null;
        _reader = null;
    }

    private void ReadLoop(SerialPort port, CancellationToken ct)
    {
        var buffer = new byte[256];

        while (!ct.IsCancellationRequested)
        {
            int read;
            try
            {
                read = port.BaseStream.Read(buffer, 0, buffer.Length);
            }
            catch (TimeoutException)
            {
                continue;
            }
            catch (Exception)
            {
                // The port went away - unplugged, or the HC-05 out of range.
                if (!ct.IsCancellationRequested)
                {
                    LinkStateChanged?.Invoke(new LinkState.Failed("The link to the socket dropped."));
                }
                return;
            }

            if (read <= 0) continue;
            _sawBytes = true;

            for (var i = 0; i < read; i++)
            {
                var c = (char)buffer[i];

                if (c == '\r') continue;

                if (c != '\n')
                {
                    lock (_gate)
                    {
                        // A line this long is not a status line. Dropping the
                        // excess stops a stuck link from growing the buffer
                        // without bound.
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
                // Anything else is dropped on purpose. The socket prints nothing
                // else once HAS_BLUETOOTH is 1, but a half line after a reset is
                // normal and is not worth reporting.
            }
        }
    }

    private void Fail(string reason) =>
        LinkStateChanged?.Invoke(new LinkState.Failed(reason));
}
