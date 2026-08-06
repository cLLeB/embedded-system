using Microsoft.Win32;
using SmartSocket.Core;

namespace SmartSocket.App;

/// <summary>
/// Cuts mains power when this laptop's own battery reaches the user's limit.
///
/// The desktop counterpart of the Android app's BatteryWatcher, and it exists
/// for a related but distinct reason. On Android the socket cannot see a phone
/// at all - 20 mA is inside the ACS712's noise - so the app is the only thing
/// that can cut. On Windows the socket *can* see a laptop, and will cut on its
/// own when the current tapers; but that happens wherever the charger decides
/// to taper, usually the high nineties. Stopping at 80% for the sake of the
/// battery's lifespan is a number only Windows knows.
///
/// So both cutoffs are live at once and they do not conflict: whichever
/// condition is met first cuts, and the firmware ignores a CUT it is already
/// in.
///
/// Owned by the application, not a window, so closing to the tray does not stop
/// it - which is the only state it is ever needed in.
/// </summary>
public sealed class BatteryWatcher : IDisposable
{
    /// <summary>
    /// Belt and braces, and the heartbeat.
    ///
    /// PowerModeChanged fires on the events that matter, but a slow discharge
    /// between two events would otherwise go unnoticed. It is also what renews
    /// the app-managed claim, which the socket drops after three minutes of
    /// silence - so this interval must stay comfortably inside that.
    /// </summary>
    private static readonly TimeSpan PollInterval = TimeSpan.FromSeconds(20);

    private readonly SocketRepository _repo;
    private readonly AppSettings _settings;
    private readonly System.Timers.Timer _timer;

    /// <summary>Set once per connection, so crossing the limit fires one cut.</summary>
    private bool _limitHandled;

    public int Percent { get; private set; } = -1;

    public bool OnAcPower { get; private set; }

    public bool HasBattery { get; private set; } = true;

    public event Action? Changed;

    /// <summary>Raised when the watcher is the thing that cut power.</summary>
    public event Action<int>? CutForLimit;

    /// <summary>Raised when it put power back after the battery fell.</summary>
    public event Action<int>? ResumedAtLimit;

    public BatteryWatcher(SocketRepository repo, AppSettings settings)
    {
        _repo = repo;
        _settings = settings;

        _timer = new System.Timers.Timer(PollInterval.TotalMilliseconds) { AutoReset = true };
        _timer.Elapsed += (_, _) => Poll();

        _repo.LinkChanged += OnLinkChanged;
    }

    public void Start()
    {
        // Instant reaction to the events Windows already knows about, rather
        // than waiting up to a poll interval to notice the charger was pulled.
        SystemEvents.PowerModeChanged += OnPowerModeChanged;

        Poll();
        _timer.Start();
    }

    public void SetLimit(int limit)
    {
        _settings.BatteryLimit = Math.Clamp(limit, 50, 100);
        _settings.Save();
        _limitHandled = false;
        Poll();
    }

    public void SetCutOnLimit(bool enabled)
    {
        _settings.CutOnLimit = enabled;
        _settings.Save();
        _limitHandled = false;

        // Turning this off must hand the decision straight back to the socket,
        // or nothing is watching the charge at all.
        _ = PushModeAsync();

        Poll();
    }

    private async void OnLinkChanged(Transport.LinkState state)
    {
        if (state is not Transport.LinkState.Connected) return;

        // A command sent over a link that was already dying may never have
        // arrived. Re-arming costs nothing - MaybeCut only acts if power is
        // still on.
        _limitHandled = false;

        // Tell the socket who is deciding, on every fresh connection rather than
        // once: the socket may have rebooted while the link was down, and it
        // comes up deciding for itself.
        await PushModeAsync().ConfigureAwait(false);

        Poll();
    }

    /// <summary>
    /// Hands the full-charge decision to this app, or gives it back.
    ///
    /// While the app is cutting on a real battery percentage, the socket's own
    /// taper rule is worse than useless - it fires on a guess made from 26 mA of
    /// resolution and cuts laptops part-charged. When the app is not doing the
    /// cutting, the socket must go straight back to deciding for itself, because
    /// then it is the only thing that can.
    /// </summary>
    private async Task PushModeAsync()
    {
        if (!_repo.IsConnected) return;
        await _repo.SendLineAsync(_settings.CutOnLimit ? "A1" : "A0").ConfigureAwait(false);
    }

    /// <summary>
    /// Hands the decision back immediately, for a deliberate disconnect or quit.
    ///
    /// The claim would lapse on its own after three minutes, but those three
    /// minutes are a hole: the socket suppresses both its taper cutoff and its
    /// recovery probe while the claim stands, so nothing at all is watching the
    /// charge. Waiting it out is fine when the client vanished without warning -
    /// there is nobody to ask. When the user pressed Disconnect there is no
    /// reason to leave the socket unattended for even a second.
    /// </summary>
    public async Task HandBackAsync()
    {
        if (!_repo.IsConnected) return;
        await _repo.SendLineAsync("A0").ConfigureAwait(false);
    }

    /// <summary>
    /// Sends the battery level for the socket's own display. Nothing in the
    /// firmware's state machine reads it - a socket with no client attached has
    /// to behave identically - but someone standing at the socket should be able
    /// to see the number the cutoff is actually based on.
    /// </summary>
    private async Task PushBatteryAsync()
    {
        if (!_repo.IsConnected) return;
        if (Percent < 0 || Percent > 100) return;
        await _repo.SendLineAsync($"B{Percent}").ConfigureAwait(false);
    }

    private void OnPowerModeChanged(object sender, PowerModeChangedEventArgs e) => Poll();

    private void Poll()
    {
        if (WindowsPower.Read() is not { } power) return;

        var changed =
            Percent != power.Percent ||
            OnAcPower != power.OnAcPower ||
            HasBattery != power.HasBattery;

        Percent = power.Percent;
        OnAcPower = power.OnAcPower;
        HasBattery = power.HasBattery;

        if (changed)
        {
            Changed?.Invoke();
            _ = PushBatteryAsync();
        }

        // Renew the claim on every poll, not just when something changes. The
        // socket drops it after three minutes of silence - deliberately, so a
        // laptop carried out of range cannot leave the socket switched off with
        // nothing able to turn it back on. Twenty seconds is comfortably inside
        // that, and the message is three bytes.
        _ = PushModeAsync();

        MaybeCut();
        MaybeResume();
    }

    private void MaybeCut()
    {
        if (!_settings.CutOnLimit) return;
        if (_limitHandled) return;
        if (!HasBattery) return;

        // -1 means Windows would not say. Guessing here would cut power to a
        // machine whose charge is unknown.
        if (Percent < 0) return;

        if (!_repo.IsConnected) return;
        if (Percent < _settings.BatteryLimit) return;
        if (!_repo.Status.State.IsPowerOn()) return;

        _limitHandled = true;
        _ = _repo.SendAsync(SocketCommand.Cut);
        CutForLimit?.Invoke(Percent);
    }

    /// <summary>
    /// Puts power back when the battery has fallen far enough.
    ///
    /// This is the half that makes the socket something you can leave a laptop
    /// on indefinitely rather than a one-shot cutoff you have to go and reset.
    /// It only ever acts on a socket this app cut - a cutoff the socket decided
    /// on its own, or one a person asked for, is not this code's to undo.
    /// </summary>
    private void MaybeResume()
    {
        if (!_settings.CutOnLimit) return;
        if (!_limitHandled) return;
        if (!HasBattery) return;
        if (Percent < 0) return;

        if (!_repo.IsConnected) return;
        if (Percent > _settings.ResumeAt) return;

        // Only from Cutoff. Re-arming out of a fault or a stuck relay would be
        // closing contacts on a socket that raised its hand for a reason.
        if (_repo.Status.State != SocketState.Cutoff) return;

        _limitHandled = false;
        _ = _repo.SendAsync(SocketCommand.Rearm);
        ResumedAtLimit?.Invoke(Percent);
    }

    public void Dispose()
    {
        SystemEvents.PowerModeChanged -= OnPowerModeChanged;
        _repo.LinkChanged -= OnLinkChanged;
        _timer.Stop();
        _timer.Dispose();
    }
}
