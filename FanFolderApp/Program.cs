using Microsoft.Win32;

namespace FanFolderApp;

static class Program
{
    internal const string RegKey   = @"SOFTWARE\FanFolder";
    internal const string RegValue = "FolderPath";

    [STAThread]
    static void Main()
    {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        Application.Run(new MainHiddenForm(LoadFolderPath()));
    }

    private static string LoadFolderPath()
    {
        // 1. Registry (primary store)
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValue) is string path
                && !string.IsNullOrWhiteSpace(path)
                && Directory.Exists(path))
                return path;
        }
        catch { }

        // 2. One-time migration from appsettings.json (previous installs)
        string? migrated = TryMigrateFromJson();
        if (migrated != null) { SaveFolderPath(migrated); return migrated; }

        // 3. Default: Downloads folder
        string fallback = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
        if (!Directory.Exists(fallback))
            fallback = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);

        SaveFolderPath(fallback);
        return fallback;
    }

    private static string? TryMigrateFromJson()
    {
        try
        {
            string jsonPath = Path.Combine(AppContext.BaseDirectory, "appsettings.json");
            if (!File.Exists(jsonPath)) return null;

            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(jsonPath));
            if (doc.RootElement.TryGetProperty("FanFolderPath", out var elem))
            {
                string? path = elem.GetString();
                if (!string.IsNullOrWhiteSpace(path) && Directory.Exists(path))
                    return path;
            }
        }
        catch { }
        return null;
    }

    /// <summary>
    /// Persists <paramref name="path"/> to the registry so it survives restarts.
    /// Called on first run and by the "Change folder…" menu item.
    /// </summary>
    internal static void SaveFolderPath(string path)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegKey, writable: true);
            key.SetValue(RegValue, path);
        }
        catch { }
    }
}