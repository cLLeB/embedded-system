using SmartSocket.Core;

namespace SmartSocket.App.Transport;

/// <summary>
/// The socket's state machine in software, so every screen can be built and
/// demonstrated with no hardware in the room.
///
/// Runs fast on purpose: a charge that takes three hours on the bench takes
/// about a minute here, which is the difference between a demo somebody
/// watches and a demo somebody leaves.
/// </summary>
public sealed class MockTransport : ISocketTransport
{
    private static readonly TimeSpan Tick = TimeSpan.FromMilliseconds(700);

    private CancellationTokenSource? _cts;

    private SocketState _state = SocketState.Calibrating;
    private int _currentMa;
    private int _peakMa;
    private int _thresholdMa;
    private long _elapsedMs;
    private int _cutoffs;
    private long _savedMs;
    private int _ticks;

    public event Action<LinkState>? LinkStateChanged;
    public event Action<SocketStatus>? StatusChanged;

    public Task<bool> ConnectAsync(SocketDevice device, CancellationToken ct = default)
    {
        Disconnect();

        LinkStateChanged?.Invoke(new LinkState.Connecting());

        _state = SocketState.Calibrating;
        _currentMa = 0;
        _peakMa = 0;
        _thresholdMa = 0;
        _elapsedMs = 0;
        _ticks = 0;

        _cts = new CancellationTokenSource();
        var token = _cts.Token;

        LinkStateChanged?.Invoke(new LinkState.Connected(device));
        _ = Task.Run(() => RunAsync(token), CancellationToken.None);

        return Task.FromResult(true);
    }

    /// <summary>
    /// Accepted and ignored. The demo has no display to update and no charge
    /// policy to hand over, so A and B lines have nothing to do here.
    /// </summary>
    public Task SendLineAsync(string line) => Task.CompletedTask;

    public Task SendAsync(SocketCommand command)
    {
        switch (command)
        {
            case SocketCommand.Cut:
                if (_state.IsPowerOn()) EnterCutoff();
                break;

            case SocketCommand.Rearm:
                _state = SocketState.Ready;
                _currentMa = 0;
                _peakMa = 0;
                _thresholdMa = 0;
                _elapsedMs = 0;
                _ticks = 0;
                break;

            case SocketCommand.Probe:
                if (_state == SocketState.Cutoff) _state = SocketState.Probing;
                break;

            case SocketCommand.StatusNow:
                Publish();
                break;
        }

        return Task.CompletedTask;
    }

    public void Disconnect()
    {
        try { _cts?.Cancel(); } catch (Exception) { /* already gone */ }
        _cts?.Dispose();
        _cts = null;
        LinkStateChanged?.Invoke(new LinkState.Idle());
    }

    private async Task RunAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            Step();
            Publish();

            try
            {
                await Task.Delay(Tick, ct).ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                return;
            }
        }
    }

    private void Step()
    {
        _ticks++;

        switch (_state)
        {
            case SocketState.Calibrating:
                if (_ticks > 3) { _state = SocketState.Ready; _ticks = 0; }
                break;

            case SocketState.Ready:
                // Something gets plugged in after a few seconds of an empty
                // socket, so the demo does not sit on one screen.
                if (_ticks > 4)
                {
                    _state = SocketState.Settling;
                    _currentMa = 150;
                    _ticks = 0;
                }
                break;

            case SocketState.Settling:
                _currentMa = 150;
                if (_ticks > 3)
                {
                    _state = SocketState.Charging;
                    _peakMa = _currentMa;
                    // The firmware's own rule: cut when the current falls to a
                    // fraction of the peak it saw.
                    _thresholdMa = (int)(_peakMa * 0.45);
                    _ticks = 0;
                }
                break;

            case SocketState.Charging:
                _elapsedMs += (long)Tick.TotalMilliseconds;
                // Taper, the way a real charger does: fast at first, flattening.
                _currentMa = Math.Max(20, (int)(_currentMa * 0.93));
                if (_currentMa > _peakMa) _peakMa = _currentMa;
                if (_currentMa <= _thresholdMa) EnterCutoff();
                break;

            case SocketState.Cutoff:
                _savedMs += (long)Tick.TotalMilliseconds;
                _currentMa = 0;
                // The socket probes its way back on a timer, looking for a new
                // load, exactly as the firmware does.
                if (_ticks > 12) { _state = SocketState.Probing; _ticks = 0; }
                break;

            case SocketState.Probing:
                _currentMa = 0;
                if (_ticks > 2) { _state = SocketState.Cutoff; _ticks = 0; }
                break;
        }
    }

    private void EnterCutoff()
    {
        _state = SocketState.Cutoff;
        _cutoffs++;
        _ticks = 0;
    }

    private void Publish() => StatusChanged?.Invoke(new SocketStatus(
        State: _state,
        CurrentMa: _currentMa,
        PeakMa: _peakMa,
        ThresholdMa: _thresholdMa,
        SessionElapsedMs: _elapsedMs,
        CutoffCount: _cutoffs,
        TotalSavedMs: _savedMs,
        RelayClosed: _state.IsPowerOn()));
}
