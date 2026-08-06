using System.IO;
using System.Text.Json;
using SmartSocket.Core;

namespace SmartSocket.App;

/// <summary>
/// Everything the app keeps between runs, under
/// <c>%APPDATA%\SmartSocket</c>.
/// </summary>
internal static class Storage
{
    public static string Directory
    {
        get
        {
            var dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "SmartSocket");
            System.IO.Directory.CreateDirectory(dir);
            return dir;
        }
    }

    public static string SettingsPath => Path.Combine(Directory, "settings.json");

    public static string HistoryPath => Path.Combine(Directory, "history.csv");
}

/// <summary>
/// User choices that must survive a restart. Written on every change rather
/// than at exit: the app is expected to be killed by a shutdown, not closed.
/// </summary>
public sealed class AppSettings
{
    public int BatteryLimit { get; set; } = 90;

    /// <summary>
    /// How far the battery must fall before power comes back.
    ///
    /// A window, not a point. Cutting at 90 and resuming at 90 would chatter the
    /// relay every time the reading crossed the line - and these are mains
    /// contacts, which weld. Ten points of hysteresis means one cut and one
    /// resume per cycle, which is also how a laptop battery prefers to be
    /// treated.
    /// </summary>
    public int ResumeGap { get; set; } = 10;

    public int ResumeAt => Math.Max(5, BatteryLimit - Math.Max(1, ResumeGap));

    public bool CutOnLimit { get; set; } = true;

    /// <summary>Reconnected to on launch. Cleared when the user disconnects.</summary>
    public string? LastPort { get; set; }

    public bool StartMinimised { get; set; }

    private static readonly JsonSerializerOptions Options = new() { WriteIndented = true };

    public static AppSettings Load()
    {
        try
        {
            if (!File.Exists(Storage.SettingsPath)) return new AppSettings();
            var json = File.ReadAllText(Storage.SettingsPath);
            return JsonSerializer.Deserialize<AppSettings>(json) ?? new AppSettings();
        }
        catch (Exception)
        {
            // A corrupt settings file must not stop the app starting. Defaults
            // are a working app; a crash on launch is not.
            return new AppSettings();
        }
    }

    public void Save()
    {
        try
        {
            File.WriteAllText(Storage.SettingsPath, JsonSerializer.Serialize(this, Options));
        }
        catch (Exception)
        {
            // Losing a preference is not worth taking the app down for.
        }
    }
}

/// <summary>
/// Completed charges, newest first on screen.
///
/// Stored as the same four-field encoding the Android app writes, one per line,
/// so a history file is portable between the two.
/// </summary>
public sealed class HistoryStore
{
    private const int MaxSessions = 200;

    private readonly List<ChargeSession> _sessions = [];

    public IReadOnlyList<ChargeSession> Sessions => _sessions;

    public event Action? Changed;

    public HistoryStore() => Load();

    public void Record(ChargeSession session)
    {
        _sessions.Insert(0, session);
        if (_sessions.Count > MaxSessions) _sessions.RemoveRange(MaxSessions, _sessions.Count - MaxSessions);
        Save();
        Changed?.Invoke();
    }

    public void Clear()
    {
        _sessions.Clear();
        Save();
        Changed?.Invoke();
    }

    /// <summary>The spreadsheet form, oldest first. Shared with Android.</summary>
    public string ToCsv() => HistoryCsv.Render(_sessions);

    private void Load()
    {
        try
        {
            if (!File.Exists(Storage.HistoryPath)) return;

            foreach (var line in File.ReadAllLines(Storage.HistoryPath))
            {
                if (ChargeSession.Decode(line) is { } session) _sessions.Add(session);
            }
        }
        catch (Exception)
        {
            // An unreadable history is an empty history, not a failed launch.
        }
    }

    private void Save()
    {
        try
        {
            File.WriteAllLines(Storage.HistoryPath, _sessions.Select(s => s.Encode()));
        }
        catch (Exception)
        {
            // Nothing the user can do about it, and the in-memory list is still
            // correct for this run.
        }
    }
}
