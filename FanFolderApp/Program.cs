using Microsoft.Win32;

namespace FanFolderApp;

static class Program
{
    internal const string RegKey          = @"SOFTWARE\FanFolder";
    internal const string RegValueFolder  = "FolderPath";
    internal const string RegValueSort    = "SortMode";
    internal const string RegValueMax     = "MaxItems";
    internal const string RegValueDirs    = "IncludeDirectories";
    internal const string RegValueRegex   = "FilterRegex";
    internal const string RegValueAnim    = "AnimationStyle";

    // Retained for back-compat (old code that references RegValue by name).
    internal const string RegValue = RegValueFolder;

    [STAThread]
    static void Main()
    {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        string   folder      = LoadFolderPath();
        SortMode sortMode    = LoadSortMode();
        int      maxItems    = LoadMaxItems();
        bool     includeDirs = LoadIncludeDirs();
        string?  filterRegex = LoadFilterRegex();
        AnimStyle animStyle  = LoadAnimStyle();

        Application.Run(new MainHiddenForm(folder, sortMode, maxItems, includeDirs, filterRegex, animStyle));
    }

    private static string LoadFolderPath()
    {
        // 1. Registry (primary store)
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValueFolder) is string path
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

    private static SortMode LoadSortMode()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValueSort) is string raw
                && Enum.TryParse<SortMode>(raw, ignoreCase: true, out var mode))
                return mode;
        }
        catch { }
        return SortMode.DateModifiedDesc;
    }

    private static int LoadMaxItems()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValueMax) is int n && n is >= 1 and <= 50)
                return n;
            // Registry stores REG_SZ on some editors — try string parse too.
            if (key?.GetValue(RegValueMax) is string s
                && int.TryParse(s, out int parsed)
                && parsed is >= 1 and <= 50)
                return parsed;
        }
        catch { }
        return 15;
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
            key.SetValue(RegValueFolder, path);
        }
        catch { }
    }

    internal static void SaveSortMode(SortMode mode)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegKey, writable: true);
            key.SetValue(RegValueSort, mode.ToString());
        }
        catch { }
    }

    internal static void SaveMaxItems(int count)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegKey, writable: true);
            key.SetValue(RegValueMax, count, RegistryValueKind.DWord);
        }
        catch { }
    }

    private static bool LoadIncludeDirs()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValueDirs) is int n) return n != 0;
            if (key?.GetValue(RegValueDirs) is string s
                && bool.TryParse(s, out bool b)) return b;
        }
        catch { }
        return true; // default: include directories
    }

    private static string? LoadFilterRegex()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValueRegex) is string s
                && !string.IsNullOrWhiteSpace(s))
                return s;
        }
        catch { }
        return null;
    }

    private static AnimStyle LoadAnimStyle()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegKey);
            if (key?.GetValue(RegValueAnim) is string raw
                && Enum.TryParse<AnimStyle>(raw, ignoreCase: true, out var style))
                return style;
        }
        catch { }
        return AnimStyle.Fan; // default
    }

    internal static void SaveIncludeDirs(bool include)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegKey, writable: true);
            key.SetValue(RegValueDirs, include ? 1 : 0, RegistryValueKind.DWord);
        }
        catch { }
    }

    internal static void SaveFilterRegex(string? pattern)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegKey, writable: true);
            key.SetValue(RegValueRegex, pattern ?? string.Empty);
        }
        catch { }
    }
}