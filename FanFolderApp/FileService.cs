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
    /// Returns true only when the icon's visible pixels span a meaningful
    /// portion of the canvas.  A "stub" HICON (e.g. SHIL_JUMBO for .zip on
    /// some Windows versions) places a tiny 32×32 image inside a 256×256
    /// canvas; the bounding box of its visible pixels is ≈ 12 % of the width,
    /// which is well below the 25 % threshold and correctly rejected.
    /// </summary>
    private static unsafe bool HasVisiblePixels(Icon icon)
    {
        try
        {
            using var bmp = icon.ToBitmap();
            int w = bmp.Width, h = bmp.Height;
            var rect = new Rectangle(0, 0, w, h);
            var data = bmp.LockBits(rect,
                System.Drawing.Imaging.ImageLockMode.ReadOnly,
                System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            try
            {
                byte* p = (byte*)data.Scan0;
                int minX = w, maxX = 0, minY = h, maxY = 0;

                // Sample every 4th pixel; alpha is byte index 3 of each ARGB dword.
                for (int y = 0; y < h; y += 4)
                {
                    byte* row = p + y * w * 4;
                    for (int x = 0; x < w; x += 4)
                    {
                        if (row[x * 4 + 3] > 8)
                        {
                            if (x < minX) minX = x;
                            if (x > maxX) maxX = x;
                            if (y < minY) minY = y;
                            if (y > maxY) maxY = y;
                        }
                    }
                }

                if (minX > maxX || minY > maxY) return false; // fully blank

                // Require the bounding box to cover ≥ 25 % of each dimension.
                int bboxW = maxX - minX + 1;
                int bboxH = maxY - minY + 1;
                return bboxW * 4 >= w && bboxH * 4 >= h;
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

