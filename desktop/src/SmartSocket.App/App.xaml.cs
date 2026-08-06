using System.Drawing;
using System.Windows;
using System.Windows.Forms;
using SmartSocket.App.Transport;
using SmartSocket.Core;
using Application = System.Windows.Application;

namespace SmartSocket.App;

/// <summary>
/// The application, and the owner of everything that must outlive the window.
///
/// Same division as the Android app: the link, the battery watch and the
/// history belong to the process, not to a screen. Closing the window hides it
/// to the tray rather than exiting, because a socket being watched is the
/// normal state and a visible window is not.
/// </summary>
public partial class App : Application
{
    public static AppSettings Settings { get; private set; } = null!;
    public static SocketRepository Repository { get; private set; } = null!;
    public static BatteryWatcher Battery { get; private set; } = null!;

    private NotifyIcon? _tray;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        // Closing the main window must not end the process - the tray icon is
        // what keeps the socket watched.
        ShutdownMode = ShutdownMode.OnExplicitShutdown;

        Settings = AppSettings.Load();
        Repository = new SocketRepository();
        Battery = new BatteryWatcher(Repository, Settings);

        SetUpTray();

        Repository.CutoffHappened += OnCutoff;
        Repository.RelayStuck += OnRelayStuck;
        Repository.LinkLost += OnLinkLost;
        Battery.CutForLimit += OnCutForLimit;
        Battery.ResumedAtLimit += OnResumedAtLimit;

        Battery.Start();

        // Pick the link back up where it was left. The equivalent of the phone
        // app's boot receiver: a machine that restarted overnight would
        // otherwise wake up with nothing watching the socket.
        if (!string.IsNullOrWhiteSpace(Settings.LastPort))
        {
            var id = Settings.LastPort!;
            _ = Task.Run(async () =>
            {
                // Look it up rather than rebuilding it: the id alone does not
                // say whether it is a COM port or a BLE device, and connecting
                // over the wrong radio would fail in a confusing way.
                var devices = await SocketRepository.AvailableDevicesAsync().ConfigureAwait(false);
                var device = devices.FirstOrDefault(d =>
                    string.Equals(d.Id, id, StringComparison.OrdinalIgnoreCase));

                if (device is not null) await Repository.ConnectAsync(device).ConfigureAwait(false);
            });
        }
    }

    private void SetUpTray()
    {
        var menu = new ContextMenuStrip();
        menu.Items.Add("Open", null, (_, _) => ShowMainWindow());
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Cut power now", null, (_, _) => _ = Repository.SendAsync(SocketCommand.Cut));
        menu.Items.Add("Re-arm", null, (_, _) => _ = Repository.SendAsync(SocketCommand.Rearm));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Quit", null, (_, _) => Shutdown());

        _tray = new NotifyIcon
        {
            Icon = LoadIcon(),
            Visible = true,
            Text = "Smart Socket",
            ContextMenuStrip = menu,
        };

        _tray.DoubleClick += (_, _) => ShowMainWindow();

        Repository.StatusChanged += status =>
        {
            // The tooltip is the only thing visible when the window is hidden,
            // so it carries the state rather than just the app name.
            var text = $"Smart Socket - {status.State.Label()}";
            // Windows truncates at 63 characters and throws above 127.
            _tray.Text = text.Length > 63 ? text[..63] : text;
        };
    }

    private static Icon LoadIcon()
    {
        try
        {
            var path = Environment.ProcessPath;
            if (path is not null && Icon.ExtractAssociatedIcon(path) is { } icon) return icon;
        }
        catch (Exception)
        {
            // Falls through to the stock icon.
        }

        return SystemIcons.Application;
    }

    private void ShowMainWindow()
    {
        var window = MainWindow ??= new MainWindow();
        window.Show();
        window.WindowState = WindowState.Normal;
        window.Activate();
    }

    private void OnCutoff(SocketStatus status) => Notify(
        "Charging finished",
        $"Power cut at {status.Amps:0.00} A, peak {status.PeakAmps:0.00} A.",
        ToolTipIcon.Info);

    private void OnRelayStuck() => Notify(
        "Relay stuck",
        "Power is still flowing with the relay commanded open. Unplug at the wall.",
        ToolTipIcon.Error);

    private void OnLinkLost(string reason) => Notify("Lost the socket", reason, ToolTipIcon.Warning);

    private void OnCutForLimit(int percent) => Notify(
        $"Battery reached {percent}%",
        $"Power cut. It will come back on at {Settings.ResumeAt}%.",
        ToolTipIcon.Info);

    private void OnResumedAtLimit(int percent) => Notify(
        $"Battery down to {percent}%",
        $"Charging again, up to {Settings.BatteryLimit}%.",
        ToolTipIcon.Info);

    private void Notify(string title, string body, ToolTipIcon icon) =>
        Dispatcher.Invoke(() => _tray?.ShowBalloonTip(5000, title, body, icon));

    protected override void OnExit(ExitEventArgs e)
    {
        // Quitting is as deliberate as pressing Disconnect, so the socket gets
        // its judgement back now rather than sitting unattended for three
        // minutes waiting for the claim to lapse. Bounded, because a shutdown
        // must not hang on a radio that has already gone.
        try { Battery.HandBackAsync().Wait(TimeSpan.FromMilliseconds(750)); }
        catch (Exception) { /* the link was already gone; the claim will lapse */ }

        Battery.Dispose();
        Repository.Dispose();

        if (_tray is not null)
        {
            _tray.Visible = false;
            _tray.Dispose();
        }

        base.OnExit(e);
    }
}
