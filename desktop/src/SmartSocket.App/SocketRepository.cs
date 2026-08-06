using SmartSocket.App.Transport;
using SmartSocket.Core;

namespace SmartSocket.App;

/// <summary>
/// Owns the link to the socket, and outlives every window showing it.
///
/// THE POINT OF THIS CLASS IS ITS LIFETIME, the same as its Android twin. Held
/// by the application rather than a window, the connection survives the main
/// window being closed to the tray - which is what makes "tell me when charging
/// finishes" possible at all.
///
/// It is also the only place that watches for state *transitions* rather than
/// state. A cutoff is recorded on the edge into Cutoff, once - not on every
/// status line that happens to say Cutoff.
/// </summary>
public sealed class SocketRepository : IDisposable
{
    private readonly SerialTransport _serial = new();
    private readonly BleTransport _ble = new();
    private readonly MockTransport _mock = new();

    private ISocketTransport _active;

    public HistoryStore History { get; } = new();

    public SocketStatus Status { get; private set; } = SocketStatus.Unknown;

    public LinkState Link { get; private set; } = new LinkState.Idle();

    public bool IsConnected => Link is LinkState.Connected;

    /// <summary>Which BLE serial profile matched, once a BLE link is up.</summary>
    public string? BleProfileName => _ble.ProfileName;

    public string DeviceName => Link switch
    {
        LinkState.Connected c => c.Device.Name,
        LinkState.Reconnecting r => r.Device.Name,
        _ => "Smart Socket",
    };

    public event Action<SocketStatus>? StatusChanged;
    public event Action<LinkState>? LinkChanged;

    /// <summary>Raised on the edge into Cutoff, once per cutoff.</summary>
    public event Action<SocketStatus>? CutoffHappened;

    /// <summary>Raised on the edge into RelayStuck. Power is still live.</summary>
    public event Action? RelayStuck;

    public event Action<string>? LinkLost;

    // --- session tracking -----------------------------------------------------

    private int _sessionPeakMa;
    private long _sessionStartedAtMs;
    private SocketState? _previousState;

    /// <summary>
    /// The socket's own lifetime cutoff count, as last seen. -1 until a first
    /// status line arrives.
    ///
    /// Watched as well as the state edge because the edge is only visible if the
    /// app happens to be connected at the instant it happens - and a socket
    /// left charging overnight does its work precisely when nothing is watching.
    /// This counter lives in the socket's EEPROM and survives both a reboot and
    /// a dropped link, so a jump in it is proof of a cutoff that was missed.
    /// </summary>
    private int _lastCutoffCount = -1;

    // --- reconnection ---------------------------------------------------------

    private SocketDevice? _target;

    /// <summary>
    /// Whether this target has ever answered. A first attempt that fails is the
    /// wrong port or a socket that is switched off - retrying it for eight
    /// minutes would only hide the error message that says so.
    /// </summary>
    private bool _everAnswered;

    private CancellationTokenSource? _reconnectCts;

    public SocketRepository()
    {
        _active = _serial;

        _serial.StatusChanged += OnStatus;
        _serial.LinkStateChanged += OnLink;
        _ble.StatusChanged += OnStatus;
        _ble.LinkStateChanged += OnLink;
        _mock.StatusChanged += OnStatus;
        _mock.LinkStateChanged += OnLink;
    }

    /// <summary>
    /// Everything the socket could be on, both radios and the cable.
    ///
    /// BLE devices are listed alongside COM ports rather than on a separate
    /// screen because the user does not know or care which radio their module
    /// uses - that is the whole problem this solves. They pick the thing called
    /// HC-05 and the right transport is chosen for them.
    /// </summary>
    public static async Task<IReadOnlyList<SocketDevice>> AvailableDevicesAsync()
    {
        var ports = SerialTransport.AvailablePorts();
        var ble = await BleTransport.PairedDevicesAsync().ConfigureAwait(false);
        return [.. ports, .. ble];
    }

    public async Task ConnectAsync(SocketDevice device)
    {
        CancelReconnect();

        _active = device.Kind switch
        {
            LinkKind.Demo => _mock,
            LinkKind.Ble => _ble,
            _ => _serial,
        };

        _target = device;
        _everAnswered = false;
        _previousState = null;

        await _active.ConnectAsync(device).ConfigureAwait(false);
    }

    public Task SendAsync(SocketCommand command) => _active.SendAsync(command);

    public Task SendLineAsync(string line) => _active.SendLineAsync(line);

    public void Disconnect()
    {
        CancelReconnect();
        _target = null;
        _everAnswered = false;
        _previousState = null;

        // Only forgotten on a deliberate hang-up, not on a dropped link. Kept
        // across a reconnect it is exactly what catches a cutoff that happened
        // while the link was down; kept across a change of socket it would
        // invent one.
        _lastCutoffCount = -1;

        _active.Disconnect();

        Status = SocketStatus.Unknown;
        StatusChanged?.Invoke(Status);
        SetLink(new LinkState.Idle());
    }

    private void OnLink(LinkState state)
    {
        switch (state)
        {
            case LinkState.Connected:
                _everAnswered = true;
                CancelReconnect();
                SetLink(state);
                break;

            case LinkState.Failed failed:
                // While the loop is running it narrates its own progress; the
                // failure of one attempt inside it is not news.
                if (_reconnectCts is { IsCancellationRequested: false }) return;

                if (_everAnswered && _target is { IsDemo: false } device)
                {
                    StartReconnecting(device);
                }
                else
                {
                    SetLink(failed);
                }
                break;

            default:
                if (_reconnectCts is null) SetLink(state);
                break;
        }
    }

    /// <summary>
    /// Rebuilds a link that dropped on its own.
    ///
    /// A laptop left charging goes out of Bluetooth range every time it is
    /// carried to another room, and the cutoff it is waiting for might be an
    /// hour away. Returning to the port picker and waiting to be noticed loses
    /// exactly the case running in the tray exists to serve.
    /// </summary>
    private void StartReconnecting(SocketDevice device)
    {
        _reconnectCts = new CancellationTokenSource();
        var ct = _reconnectCts.Token;

        _ = Task.Run(async () =>
        {
            var attempt = 0;

            while (!ct.IsCancellationRequested)
            {
                if (ReconnectPolicy.DelayMsFor(attempt) is not { } wait) break;

                SetLink(new LinkState.Reconnecting(device, attempt + 1));

                try
                {
                    await Task.Delay(TimeSpan.FromMilliseconds(wait), ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    return;
                }

                var transport = device.Kind == LinkKind.Ble
                    ? (ISocketTransport)_ble
                    : _serial;

                if (await transport.ConnectAsync(device, ct).ConfigureAwait(false)) return;
                attempt++;
            }

            if (ct.IsCancellationRequested) return;

            _reconnectCts = null;
            var reason = $"Lost the link to {device.Name} and could not get it back.";
            SetLink(new LinkState.Failed(reason));

            // Nobody is looking at the window - that is the whole premise of
            // reconnecting at all - so silence here means the user finds out
            // when the charge they were told would be watched was not.
            LinkLost?.Invoke(reason);
        }, CancellationToken.None);
    }

    private void CancelReconnect()
    {
        try { _reconnectCts?.Cancel(); } catch (Exception) { /* already gone */ }
        _reconnectCts?.Dispose();
        _reconnectCts = null;
    }

    private void OnStatus(SocketStatus status)
    {
        Status = status;
        StatusChanged?.Invoke(status);

        if (status.PeakMa > _sessionPeakMa) _sessionPeakMa = status.PeakMa;

        if (status.State == SocketState.Settling && _previousState != SocketState.Settling)
        {
            _sessionPeakMa = 0;
            _sessionStartedAtMs = Now();
        }

        // A cutoff the socket decided for itself while nobody was connected. Its
        // counter only moves on a real one, so a jump is evidence even though
        // the transition itself was never seen.
        var missedACutoff = _lastCutoffCount >= 0 && status.CutoffCount > _lastCutoffCount;
        _lastCutoffCount = status.CutoffCount;

        var was = _previousState;
        _previousState = status.State;

        var enteredCutoff = was is not null
                            && was != status.State
                            && status.State == SocketState.Cutoff;

        // Either signal records once, never twice: a cutoff seen live moves the
        // counter in the same status line that carries the transition.
        if (enteredCutoff || missedACutoff)
        {
            History.Record(new ChargeSession(
                EndedAtMillis: Now(),
                PeakMa: _sessionPeakMa,
                CutAtMa: status.CurrentMa,
                DurationMs: _sessionStartedAtMs > 0 ? Now() - _sessionStartedAtMs : 0L));

            CutoffHappened?.Invoke(status with { PeakMa = _sessionPeakMa });

            _sessionPeakMa = 0;
            _sessionStartedAtMs = 0;
        }

        if (was is null || was == status.State) return;

        if (status.State == SocketState.RelayStuck) RelayStuck?.Invoke();
    }

    private void SetLink(LinkState state)
    {
        Link = state;
        LinkChanged?.Invoke(state);
    }

    private static long Now() => DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

    public void Dispose()
    {
        CancelReconnect();
        _serial.Disconnect();
        _ble.Disconnect();
        _mock.Disconnect();
    }
}
