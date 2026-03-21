using System.Runtime.InteropServices;

namespace FanFolderApp;

/// <summary>
/// Provides file-system operations: fetching top-N recent items and
/// extracting native shell icons via SHGetImageList.
/// </summary>
internal sealed class FileService
{
    private const int MaxItems = 15;

    /// <summary>
    /// Returns the 15 most recently modified file-system entries in the
    /// given folder, sorted descending by LastWriteTime (newest first).
    /// The caller is expected to reverse display order so the newest
    /// item appears at the bottom (nearest the taskbar).
    /// </summary>
    public static List<FileSystemInfo> GetRecentItems(string folderPath)
    {
        if (!Directory.Exists(folderPath))
            return [];

        var dir = new DirectoryInfo(folderPath);

        return dir.GetFileSystemInfos()
            .Where(f => (f.Attributes & FileAttributes.Hidden) == 0)
            .OrderByDescending(f => f.LastWriteTime)
            .Take(MaxItems)
            .ToList();
    }

    /// <summary>
    /// Extracts a shell icon for the given path.  Tries SHIL_JUMBO (256×256),
    /// SHIL_EXTRALARGE (48×48), then SHIL_LARGE (32×32) in sequence, skipping
    /// any size that produces a blank icon (some file types such as .zip have
    /// no registered icon at SHIL_JUMBO and the shell returns an empty HICON).
    /// Falls back to <see cref="Icon.ExtractAssociatedIcon"/> as a last resort.
    /// The returned Icon is owned by the caller and must be disposed.
    /// </summary>
    public static Icon? GetShellIcon(string path)
    {
        try
        {
            var shinfo = new NativeMethods.SHFILEINFO();
            uint flags = NativeMethods.SHGFI_SYSICONINDEX;

            IntPtr hr = NativeMethods.SHGetFileInfo(
                path, 0, ref shinfo, (uint)Marshal.SizeOf(shinfo), flags);

            if (hr == IntPtr.Zero)
                return FallbackIcon(path);

            int iconIndex = shinfo.iIcon;
            const int ILD_TRANSPARENT = 0x00000001;

            // Try each image list from largest to smallest.
            // SHIL_JUMBO always succeeds on Win10+ but some file types (e.g. .zip)
            // return a blank HICON at that size — skip those and try a smaller list.
            ReadOnlySpan<int> sizes = [NativeMethods.SHIL_JUMBO, NativeMethods.SHIL_EXTRALARGE, NativeMethods.SHIL_LARGE];
            foreach (int shil in sizes)
            {
                Guid iid = NativeMethods.IID_IImageList;
                if (NativeMethods.SHGetImageList(shil, ref iid, out var imageList) != 0 || imageList == null)
                    continue;

                IntPtr hIcon = IntPtr.Zero;
                if (imageList.GetIcon(iconIndex, ILD_TRANSPARENT, ref hIcon) != 0 || hIcon == IntPtr.Zero)
                    continue;

                Icon cloned = (Icon)Icon.FromHandle(hIcon).Clone();
                NativeMethods.DestroyIcon(hIcon);

                if (HasVisiblePixels(cloned))
                    return cloned;

                cloned.Dispose(); // blank — try next size
            }

            return FallbackIcon(path);
        }
        catch
        {
            return FallbackIcon(path);
        }
    }

    /// <summary>
    /// Scans every 8th pixel for any alpha value &gt; 8.
    /// Quickly detects blank HICON instances (e.g. SHIL_JUMBO for .zip files)
    /// without scanning the full bitmap.
    /// </summary>
    private static unsafe bool HasVisiblePixels(Icon icon)
    {
        try
        {
            using var bmp = icon.ToBitmap();
            var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
            var data = bmp.LockBits(rect,
                System.Drawing.Imaging.ImageLockMode.ReadOnly,
                System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            try
            {
                byte* p = (byte*)data.Scan0;
                int n = bmp.Width * bmp.Height;
                // Step 8 pixels at a time; alpha is byte 3 of each 4-byte pixel.
                for (int i = 0; i < n; i += 8)
                    if (p[i * 4 + 3] > 8) return true;
                return false;
            }
            finally { bmp.UnlockBits(data); }
        }
        catch { return true; } // assume valid if check fails
    }

    public static Icon? FallbackIcon(string path)
    {
        try
        {
            return Icon.ExtractAssociatedIcon(path);
        }
        catch
        {
            return null;
        }
    }
}

