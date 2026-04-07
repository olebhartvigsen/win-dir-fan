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
    /// Returns the best-quality <see cref="Bitmap"/> for display at
    /// <paramref name="targetSize"/>×<paramref name="targetSize"/> pixels.
    ///
    /// Primary path: <c>IShellItemImageFactory::GetImage</c> with
    /// <c>SIIGBF_ICONONLY</c> — the modern Shell API that always returns the
    /// registered icon (never a thumbnail preview), at exactly the requested
    /// size, for every file type including .zip and .7z.
    ///
    /// Fallback: <see cref="GetShellIcon"/> + bicubic scaling.
    /// </summary>
    public static Bitmap? GetShellBitmap(string path, int targetSize)
    {
        // ── Primary: IShellItemImageFactory ─────────────────────────────
        var direct = TryShellItemImage(path, targetSize);
        if (direct != null) return direct;

        // ── Fallback: existing SHIL / SHDefExtractIcon chain ────────────
        using var icon = GetShellIcon(path);
        if (icon == null) return null;

        var dst = new Bitmap(targetSize, targetSize,
            System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        using var g = Graphics.FromImage(dst);
        g.Clear(Color.Transparent);
        g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode   = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
        using var src = icon.ToBitmap();
        g.DrawImage(src, 0, 0, targetSize, targetSize);
        return dst;
    }

    /// <summary>
    /// Calls <c>IShellItemImageFactory::GetImage</c> with
    /// <c>SIIGBF_ICONONLY</c> to retrieve the shell icon as a
    /// <see cref="Bitmap"/> at exactly <paramref name="size"/>×<paramref name="size"/>
    /// pixels.  Returns null on any failure.
    /// </summary>
    private static Bitmap? TryShellItemImage(string path, int size)
    {
        try
        {
            var iid = NativeMethods.IID_IShellItemImageFactory;
            int hr = NativeMethods.SHCreateItemFromParsingName(
                path, IntPtr.Zero, ref iid, out var obj);
            if (hr != 0 || obj is not NativeMethods.IShellItemImageFactory factory)
                return null;

            var sz = new NativeMethods.SIZE { cx = size, cy = size };
            hr = factory.GetImage(sz, NativeMethods.SIIGBF_ICONONLY, out IntPtr hbm);
            if (hr != 0 || hbm == IntPtr.Zero) return null;

            try   { return HBitmapToBitmap(hbm, size); }
            finally { NativeMethods.DeleteObject(hbm); }
        }
        catch { return null; }
    }

    /// <summary>
    /// Reads the raw premultiplied-BGRA bytes from a 32 bpp HBITMAP via
    /// <c>GetDIBits</c> and wraps them in a <see cref="Bitmap"/> using
    /// <c>Format32bppPArgb</c> so per-pixel alpha is preserved correctly.
    /// </summary>
    private static Bitmap? HBitmapToBitmap(IntPtr hbm, int size)
    {
        var bmi = new NativeMethods.BITMAPINFO
        {
            bmiHeader = new NativeMethods.BITMAPINFOHEADER
            {
                biSize        = Marshal.SizeOf<NativeMethods.BITMAPINFOHEADER>(),
                biWidth       = size,
                biHeight      = -size, // top-down (positive = bottom-up)
                biPlanes      = 1,
                biBitCount    = 32,
                biCompression = 0,     // BI_RGB
            },
            bmiColors = new int[1]
        };

        byte[] bits = new byte[size * size * 4];
        IntPtr hdc = NativeMethods.GetDC(IntPtr.Zero);
        int rows = NativeMethods.GetDIBits(hdc, hbm, 0, (uint)size, bits, ref bmi, 0);
        NativeMethods.ReleaseDC(IntPtr.Zero, hdc);
        if (rows == 0) return null;

        // Copy raw premultiplied BGRA bytes straight into a PArgb bitmap.
        // GDI and GDI+ both use BGRA byte order for 32 bpp bitmaps so no
        // channel-swapping is needed.
        var dst = new Bitmap(size, size,
            System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        var rect = new Rectangle(0, 0, size, size);
        var data = dst.LockBits(rect,
            System.Drawing.Imaging.ImageLockMode.WriteOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        try   { Marshal.Copy(bits, 0, data.Scan0, bits.Length); }
        finally { dst.UnlockBits(data); }
        return dst;
    }


    /// <summary>
    /// Returns a shell icon for use in drag-and-drop operations.
    /// Uses the SHIL image-list chain (JUMBO → EXTRALARGE → LARGE) and
    /// falls back to <see cref="Icon.ExtractAssociatedIcon"/>.
    /// The returned Icon is owned by the caller and must be disposed.
    /// </summary>
    public static Icon? GetShellIcon(string path)
    {
        try
        {
            var info = new NativeMethods.SHFILEINFO();
            IntPtr hr = NativeMethods.SHGetFileInfo(
                path, 0, ref info, (uint)Marshal.SizeOf(info),
                NativeMethods.SHGFI_SYSICONINDEX);

            if (hr == IntPtr.Zero) return FallbackIcon(path);

            int iconIndex = info.iIcon;
            const int ILD_TRANSPARENT = 0x00000001;

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

                // Only apply the stub check for SHIL_JUMBO — smaller image lists
                // always fill their canvas and must never be rejected.
                if (shil == NativeMethods.SHIL_JUMBO && !HasVisiblePixels(cloned))
                {
                    cloned.Dispose();
                    continue;
                }

                return cloned;
            }

            return FallbackIcon(path);
        }
        catch
        {
            return FallbackIcon(path);
        }
    }

    /// <summary>
    /// Returns true only when the icon's visible pixels span ≥ 50 % of the canvas
    /// in each dimension.  Rejects "stub" HICONs that place a tiny graphic inside
    /// a large canvas (e.g. SHIL_JUMBO for .zip on some Windows versions).
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

                if (minX > maxX || minY > maxY) return false;
                return (maxX - minX + 1) * 2 >= w && (maxY - minY + 1) * 2 >= h;
            }
            finally { bmp.UnlockBits(data); }
        }
        catch { return true; }
    }

    public static Icon? FallbackIcon(string path)
    {
        try   { return Icon.ExtractAssociatedIcon(path); }
        catch { return null; }
    }
}
