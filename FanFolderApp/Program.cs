using Microsoft.Extensions.Configuration;

namespace FanFolderApp;

static class Program
{
    [STAThread]
    static void Main()
    {
        // High-DPI + visual styles
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        // Load configuration
        string folderPath = LoadFolderPath();

        // Run with a hidden main form (keeps the taskbar icon alive)
        Application.Run(new MainHiddenForm(folderPath));
    }

    private static string LoadFolderPath()
    {
        try
        {
            var config = new ConfigurationBuilder()
                .SetBasePath(AppContext.BaseDirectory)
                .AddJsonFile("appsettings.json", optional: false, reloadOnChange: false)
                .Build();

            string? path = config["FanFolderPath"];

            if (!string.IsNullOrWhiteSpace(path))
                return path;
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Failed to load appsettings.json:\n{ex.Message}",
                "Fan Folder – Configuration Error",
                MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
        }

        // Fallback to user's Desktop
        return Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
    }
}