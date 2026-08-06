using System.ComponentModel;
using System.IO;
using System.Windows;
using System.Windows.Media;
using SmartSocket.App.Transport;
using SmartSocket.Core;
using MessageBox = System.Windows.MessageBox;

namespace SmartSocket.App;

/// <summary>
/// The whole UI. Three views in one shell, switched by visibility rather than
/// by navigation - there are only three, and every one of them wants the same
/// header.
///
/// This window owns no state that matters. The link, the battery watch and the
/// history all live on <see cref="App"/>, so closing it loses nothing.
/// </summary>
public partial class MainWindow : Window
{
    private enum View { Connect, Dashboard, History }

    private static SocketRepository Repo => App.Repository;
    private static BatteryWatcher Battery => App.Battery;
    private static AppSettings Settings => App.Settings;

    /// <summary>Guards the input handlers while the UI is being seeded.</summary>
    private bool _loading = true;

    public MainWindow()
    {
        InitializeComponent();

        LimitSlider.Value = Settings.BatteryLimit;
        LimitLabel.Text = $"Charge to {Settings.BatteryLimit}%, back on at {Settings.ResumeAt}%";
        CutOnLimitBox.IsChecked = Settings.CutOnLimit;
        _loading = false;

        Repo.StatusChanged += OnStatus;
        Repo.LinkChanged += OnLink;
        Repo.History.Changed += OnHistoryChanged;
        Battery.Changed += OnBatteryChanged;

        Closing += OnClosing;

        RefreshPorts();
        RenderBattery();
        RenderLink(Repo.Link);
        RenderStatus(Repo.Status);
        ShowView(Repo.IsConnected ? View.Dashboard : View.Connect);
    }

    // --- view switching -------------------------------------------------------

    private void ShowView(View view)
    {
        ConnectView.Visibility = view == View.Connect ? Visibility.Visible : Visibility.Collapsed;
        DashboardView.Visibility = view == View.Dashboard ? Visibility.Visible : Visibility.Collapsed;
        HistoryView.Visibility = view == View.History ? Visibility.Visible : Visibility.Collapsed;
    }

    /// <summary>
    /// Hides rather than exits. The socket is still being watched and the tray
    /// icon says so - quitting is a deliberate act from that menu.
    /// </summary>
    private void OnClosing(object? sender, CancelEventArgs e)
    {
        e.Cancel = true;
        Hide();
    }

    // --- repository events, marshalled onto the UI thread ---------------------

    private void OnStatus(SocketStatus status) => Dispatcher.Invoke(() => RenderStatus(status));

    private void OnBatteryChanged() => Dispatcher.Invoke(RenderBattery);

    private void OnHistoryChanged() => Dispatcher.Invoke(RenderHistory);

    private void OnLink(LinkState state) => Dispatcher.Invoke(() =>
    {
        RenderLink(state);

        // The history view is never yanked out from under the user by a link
        // event - it is the one screen that does not depend on being connected.
        if (HistoryView.Visibility == Visibility.Visible) return;

        switch (state)
        {
            case LinkState.Connected:
                ConnectError.Text = "";
                ShowView(View.Dashboard);
                break;

            case LinkState.Failed failed:
                ConnectError.Text = failed.Reason;
                ShowView(View.Connect);
                break;

            case LinkState.Idle:
                ShowView(View.Connect);
                break;

            // Reconnecting deliberately leaves the view alone. Bouncing back to
            // the port picker would throw away the case the tray exists for.
        }
    });

    // --- rendering ------------------------------------------------------------

    private void RenderLink(LinkState state) => LinkLine.Text = state switch
    {
        // Naming the BLE profile that matched: on a module sold under someone
        // else's name, that string is the most reliable identification of what
        // it actually is.
        LinkState.Connected c when c.Device.Kind == LinkKind.Ble =>
            $"Connected to {c.Device.Name} over Bluetooth LE ({Repo.BleProfileName ?? "unknown profile"})",

        LinkState.Connected c => $"Connected to {c.Device.Name}",
        LinkState.Connecting => "Connecting…",
        LinkState.Reconnecting r =>
            $"Lost the link. Trying again — attempt {r.Attempt} of {ReconnectPolicy.MaxAttempts}",
        _ => "Not connected",
    };

    private void RenderStatus(SocketStatus status)
    {
        StateLabel.Text = status.State.Label();
        StateLabel.Foreground = Brush(status.State.IsAlarm() ? "Alarm" : "Gold");
        StateDetail.Text = status.State.Detail();

        CurrentReadout.Text = $"{status.Amps:0.00} A";

        // The firmware zeroes peak, threshold and elapsed outside Charging on
        // purpose - holding the last session's figures there would show old
        // numbers as if they were live. So a dash means "no charge session is
        // running", not "unknown".
        PeakReadout.Text = status.PeakMa > 0
            ? $"{status.PeakAmps:0.00} A"
            : "—";

        // While the app is managing, the socket's current threshold is not what
        // cuts the power - the battery percentage is, and the threshold is not
        // even consulted. Showing a current here would be describing a rule that
        // is switched off.
        ThresholdReadout.Text = Settings.CutOnLimit
            ? $"{Settings.BatteryLimit}%"
            : status.ThresholdMa > 0
                ? $"{status.ThresholdAmps:0.00} A"
                : "—";

        RelayReadout.Text = status.RelayClosed ? "CLOSED" : "OPEN";
        RelayReadout.Foreground = Brush(status.RelayClosed ? "Live" : "Muted");

        SessionReadout.Text = status.SessionElapsedMs > 0
            ? $"Charging for {Duration(status.SessionElapsedMs)}"
            : "No charge session running";

        SavedReadout.Text =
            $"{status.CutoffCount} cutoff{(status.CutoffCount == 1 ? "" : "s")}, " +
            $"{Duration(status.TotalSavedMs)} of power saved";
    }

    private void RenderBattery()
    {
        if (!Battery.HasBattery)
        {
            BatteryReadout.Text = "No battery";
            BatteryNote.Text =
                "This machine has no battery, so there is no percentage to cut at. " +
                "The socket still cuts on its own when the current tapers.";
            CutOnLimitBox.IsEnabled = false;
            LimitSlider.IsEnabled = false;
            return;
        }

        BatteryReadout.Text = Battery.Percent >= 0 ? $"{Battery.Percent}%" : "—";
        BatteryReadout.Foreground =
            Brush(Battery.Percent >= Settings.BatteryLimit ? "Live" : "Paper");

        BatteryNote.Text = Settings.CutOnLimit
            ? $"Cuts at {Settings.BatteryLimit}%, charges again at {Settings.ResumeAt}%. " +
              (Battery.OnAcPower
                  ? "The socket's own guesswork is switched off while this is on."
                  : "On battery — nothing to cut until the charger is plugged back in.")
            : "The socket decides for itself, from the charge current. Less accurate on a laptop.";
    }

    private void RenderHistory()
    {
        HistoryList.Items.Clear();

        if (Repo.History.Sessions.Count == 0)
        {
            HistoryList.Items.Add("Nothing recorded yet.");
            return;
        }

        foreach (var s in Repo.History.Sessions)
        {
            var when = DateTimeOffset.FromUnixTimeMilliseconds(s.EndedAtMillis).LocalDateTime;
            HistoryList.Items.Add(
                $"{when:yyyy-MM-dd HH:mm}   peak {s.PeakAmps:0.00} A   " +
                $"cut at {s.CutAtMa / 1000f:0.00} A ({s.TaperFraction * 100:0}%)   " +
                Duration(s.DurationMs));
        }
    }

    private Brush Brush(string key) => (Brush)FindResource(key);

    private static string Duration(long ms)
    {
        var span = TimeSpan.FromMilliseconds(ms);
        if (span.TotalHours >= 1) return $"{(int)span.TotalHours}h {span.Minutes}m";
        if (span.TotalMinutes >= 1) return $"{span.Minutes}m {span.Seconds}s";
        return $"{span.Seconds}s";
    }

    // --- connect view ---------------------------------------------------------

    /// <summary>What is currently in the list, in the order it is shown.</summary>
    private List<SocketDevice> _devices = [];

    private async void RefreshPorts()
    {
        PortList.Items.Clear();
        PortList.IsEnabled = false;
        PortList.Items.Add("Looking…");

        // Enumerating BLE devices goes out to the Bluetooth radio, so it cannot
        // be done on the UI thread the way listing COM ports can.
        _devices = [.. await SocketRepository.AvailableDevicesAsync()];

        PortList.Items.Clear();

        if (_devices.Count == 0)
        {
            PortList.Items.Add("Nothing found — no COM ports and no paired Bluetooth devices");
            return;
        }

        PortList.IsEnabled = true;
        foreach (var d in _devices) PortList.Items.Add(d.Display);

        // Preselect what was used last, so reconnecting is one click.
        if (Settings.LastPort is { } last)
        {
            var index = _devices.FindIndex(d => d.Id == last);
            if (index >= 0) PortList.SelectedIndex = index;
        }

        if (PortList.SelectedIndex < 0) PortList.SelectedIndex = 0;
    }

    private void OnRefreshPorts(object sender, RoutedEventArgs e) => RefreshPorts();

    private async void OnConnect(object sender, RoutedEventArgs e)
    {
        if (!PortList.IsEnabled) return;

        var index = PortList.SelectedIndex;
        if (index < 0 || index >= _devices.Count) return;

        var device = _devices[index];

        ConnectError.Text = "";
        Settings.LastPort = device.Id;
        Settings.Save();

        await Repo.ConnectAsync(device);
    }

    private async void OnDemo(object sender, RoutedEventArgs e)
    {
        ConnectError.Text = "";
        await Repo.ConnectAsync(SocketDevice.Demo);
    }

    // --- dashboard ------------------------------------------------------------

    private async void OnCut(object sender, RoutedEventArgs e) =>
        await Repo.SendAsync(SocketCommand.Cut);

    private async void OnRearm(object sender, RoutedEventArgs e) =>
        await Repo.SendAsync(SocketCommand.Rearm);

    private async void OnProbe(object sender, RoutedEventArgs e) =>
        await Repo.SendAsync(SocketCommand.Probe);

    private async void OnDisconnect(object sender, RoutedEventArgs e)
    {
        // Give the socket its own judgement back before dropping the link, so it
        // is never left with nothing watching the charge. A link that dies on
        // its own cannot do this, which is exactly why the claim also expires on
        // a timer.
        await Battery.HandBackAsync();

        // Hanging up is a decision, and it has to survive a restart too -
        // otherwise the app would reconnect on launch to a socket the user
        // deliberately walked away from.
        Settings.LastPort = null;
        Settings.Save();

        Repo.Disconnect();
        RefreshPorts();
        ShowView(View.Connect);
    }

    private void OnLimitChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_loading) return;

        var limit = (int)e.NewValue;
        Battery.SetLimit(limit);
        LimitLabel.Text = $"Charge to {Settings.BatteryLimit}%, back on at {Settings.ResumeAt}%";
        RenderBattery();
    }

    private void OnCutOnLimitChanged(object sender, RoutedEventArgs e)
    {
        if (_loading) return;
        Battery.SetCutOnLimit(CutOnLimitBox.IsChecked == true);
        RenderBattery();

        // CUTS AT changes meaning with this switch, so it has to be redrawn now
        // rather than waiting for the next status line a second later.
        RenderStatus(Repo.Status);
    }

    // --- history --------------------------------------------------------------

    private void OnOpenHistory(object sender, RoutedEventArgs e)
    {
        RenderHistory();
        ShowView(View.History);
    }

    private void OnCloseHistory(object sender, RoutedEventArgs e) =>
        ShowView(Repo.IsConnected ? View.Dashboard : View.Connect);

    private void OnClearHistory(object sender, RoutedEventArgs e)
    {
        var answer = MessageBox.Show(
            "Delete every recorded charge, and reset the socket's lifetime cutoff " +
            "count and saved time?\n\nThis cannot be undone. The socket's learned " +
            "taper calibration is kept.",
            "Clear history",
            MessageBoxButton.YesNo,
            MessageBoxImage.Warning);

        if (answer != MessageBoxResult.Yes) return;

        Repo.History.Clear();

        // The totals live in the socket's EEPROM, not here. Clearing only this
        // app's list left them on screen unchanged and looking stuck.
        if (Repo.IsConnected) _ = Repo.SendAsync(SocketCommand.ResetTotals);
    }

    private void OnExport(object sender, RoutedEventArgs e)
    {
        if (Repo.History.Sessions.Count == 0)
        {
            MessageBox.Show("There is nothing to export yet.", "Export");
            return;
        }

        var dialog = new Microsoft.Win32.SaveFileDialog
        {
            FileName = HistoryCsv.FileName,
            DefaultExt = ".csv",
            Filter = "Comma-separated values (*.csv)|*.csv",
        };

        if (dialog.ShowDialog() != true) return;

        try
        {
            File.WriteAllText(dialog.FileName, Repo.History.ToCsv());
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Could not write the file.\n\n{ex.Message}", "Export");
        }
    }
}
