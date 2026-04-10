using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

namespace FanFolderApp;

/// <summary>
/// Controls the order in which items appear in the fan menu.
/// Values mirror the sort options available in Windows Explorer.
/// </summary>
public enum SortMode
{
    DateModifiedDesc,   // Default — most recently modified first
    DateModifiedAsc,    // Oldest modification first
    NameAsc,            // File name A → Z
    NameDesc,           // File name Z → A
    SizeDesc,           // Largest first
    SizeAsc,            // Smallest first
    TypeAsc,            // File extension / type A → Z
    TypeDesc,           // File extension / type Z → A
    DateCreatedDesc,    // Most recently created first
    DateCreatedAsc,     // Oldest creation first
}

/// <summary>
/// Controls the opening animation played when the fan menu appears.
/// </summary>
public enum AnimStyle
{
    Fan,     // Items radiate one-by-one from the taskbar edge (default)
    Glide,   // All items drift upward together while fading in
    Spring,  // Items spring-scale into place with elastic overshoot
    None,    // No animation — instant appearance
}

/// <summary>
/// Provides file-system operations: fetching top-N recent items and
/// extracting native shell icons via SHGetImageList.
/// </summary>
internal sealed class FileService
{
    /// <summary>
    /// Returns up to <paramref name="maxItems"/> file-system entries from
    /// <paramref name="folderPath"/>, ordered according to
    /// <paramref name="sort"/>.  Hidden items are excluded.
    /// Directories are excluded when <paramref name="includeDirs"/> is false.
    /// When <paramref name="filterRegex"/> is non-empty, only entries whose
    /// full path matches the pattern are included.
    /// </summary>
    public static List<FileSystemInfo> GetRecentItems(
        string  folderPath,
        SortMode sort        = SortMode.DateModifiedDesc,
        int      maxItems    = 15,
        bool     includeDirs = true,
        string?  filterRegex = null)
    {
        if (!Directory.Exists(folderPath))
            return [];

        Regex? regex = null;
        if (!string.IsNullOrWhiteSpace(filterRegex))
        {
            try { regex = new Regex(filterRegex, RegexOptions.IgnoreCase | RegexOptions.Compiled); }
            catch { /* invalid pattern — ignore filter */ }
        }

        var entries = new DirectoryInfo(folderPath)
            .GetFileSystemInfos()
            .Where(f => (f.Attributes & FileAttributes.Hidden) == 0)
            .Where(f => includeDirs || f is FileInfo)
            .Where(f => regex == null || regex.IsMatch(f.FullName));

        IOrderedEnumerable<FileSystemInfo> ordered = sort switch
        {
            SortMode.DateModifiedAsc  => entries.OrderBy(f => f.LastWriteTime),
            SortMode.NameAsc          => entries.OrderBy(f => f.Name, StringComparer.OrdinalIgnoreCase),
            SortMode.NameDesc         => entries.OrderByDescending(f => f.Name, StringComparer.OrdinalIgnoreCase),
            SortMode.SizeDesc         => entries.OrderByDescending(f => (f as FileInfo)?.Length ?? 0),
            SortMode.SizeAsc          => entries.OrderBy(f => (f as FileInfo)?.Length ?? 0),
            SortMode.TypeAsc          => entries.OrderBy(f => Path.GetExtension(f.Name), StringComparer.OrdinalIgnoreCase),
            SortMode.TypeDesc         => entries.OrderByDescending(f => Path.GetExtension(f.Name), StringComparer.OrdinalIgnoreCase),
            SortMode.DateCreatedDesc  => entries.OrderByDescending(f => f.CreationTime),
            SortMode.DateCreatedAsc   => entries.OrderBy(f => f.CreationTime),
            _                         => entries.OrderByDescending(f => f.LastWriteTime), // DateModifiedDesc
        };

        return ordered.Take(maxItems).ToList();
    }

    // Extensions handled by GDI+ natively
    private static readonly HashSet<string> GdiImageExts = new(StringComparer.OrdinalIgnoreCase)
    {
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".tif", ".webp"
    };

    /// <summary>
    /// For image files, returns a scaled thumbnail of the actual image content
    /// centred in a <paramref name="targetSize"/>×<paramref name="targetSize"/>
    /// transparent square.  Returns <c>null</c> if the path is not a recognised
    /// image type or the file cannot be decoded.
    /// </summary>
    public static Bitmap? GetImageThumbnail(string path, int targetSize)
    {
        var ext = Path.GetExtension(path);
        if (GdiImageExts.Contains(ext))
            return TryGdiThumbnail(path, targetSize);
        return null;
    }

    private static Bitmap? TryGdiThumbnail(string path, int targetSize)
    {
        try
        {
            using var src = Image.FromFile(path);
            return ScaleToSquare(src, targetSize);
        }
        catch { return null; }
    }

    /// <summary>
    /// Scales <paramref name="src"/> to fit inside a
    /// <paramref name="targetSize"/>×<paramref name="targetSize"/> square
    /// while preserving aspect ratio, centred on a transparent background.
    /// </summary>
    private static Bitmap ScaleToSquare(Image src, int targetSize)
    {
        float scale = Math.Min((float)targetSize / src.Width, (float)targetSize / src.Height);
        int dw = (int)(src.Width  * scale);
        int dh = (int)(src.Height * scale);
        int ox = (targetSize - dw) / 2;
        int oy = (targetSize - dh) / 2;

        var bmp = new Bitmap(targetSize, targetSize,
            System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.Transparent);
        g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode   = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
        g.DrawImage(src, ox, oy, dw, dh);
        return bmp;
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
        // Some shell extensions (e.g. Office, SVG handlers) ignore SIIGBF_ICONONLY
        // and return a document content preview instead of the registered app icon.
        // A real icon has a transparent background; a preview is fully opaque.
        // LooksLikeIcon() rejects opaque-cornered bitmaps so they fall through to SHIL.
        var direct = TryShellItemImage(path, targetSize);
        if (direct != null && BitmapHasVisiblePixels(direct) && LooksLikeIcon(direct))
            return direct;
        direct?.Dispose();

        // ── Fallback: SHIL / SHDefExtractIcon chain ──────────────────────
        // Convert the HICON to a bitmap, then strip any transparent padding
        // before scaling up to targetSize (e.g. .zip EXTRALARGE icon is 48px
        // but may have a smaller graphic centred inside it).
        using var icon = GetShellIcon(path);
        if (icon == null) return null;

        using var src = icon.ToBitmap();
        var content = FindContentRect(src);

        var bmp = new Bitmap(targetSize, targetSize,
            System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.Transparent);
        g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode   = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
        g.DrawImage(src,
            new Rectangle(0, 0, targetSize, targetSize),
            content,
            GraphicsUnit.Pixel);
        return bmp;
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

            try
            {
                var sz = new NativeMethods.SIZE { cx = size, cy = size };
                hr = factory.GetImage(sz, NativeMethods.SIIGBF_ICONONLY, out IntPtr hbm);
                if (hr != 0 || hbm == IntPtr.Zero) return null;

                try
                {
                    return HBitmapToBitmap(hbm, size);
                }
                finally { NativeMethods.DeleteObject(hbm); }
            }
            finally { Marshal.ReleaseComObject(factory); }
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

        // Copy raw BGRA bytes into a PArgb bitmap (no channel-swap needed).
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
    /// Returns the bounding rectangle of visible (alpha &gt; 8) pixels in the
    /// bitmap.  If the content already covers the full bitmap, the full rect
    /// is returned unchanged.
    /// </summary>
    private static unsafe Rectangle FindContentRect(Bitmap bmp)
    {
        int w = bmp.Width, h = bmp.Height;
        int minX = w, maxX = 0, minY = h, maxY = 0;

        var rect = new Rectangle(0, 0, w, h);
        var data = bmp.LockBits(rect,
            System.Drawing.Imaging.ImageLockMode.ReadOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        try
        {
            byte* p = (byte*)data.Scan0;
            for (int y = 0; y < h; y++)
            {
                byte* row = p + y * w * 4;
                for (int x = 0; x < w; x++)
                    if (row[x * 4 + 3] > 8)
                    {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
            }
        }
        finally { bmp.UnlockBits(data); }

        if (minX > maxX || minY > maxY) return rect; // fully transparent — use full rect
        return new Rectangle(minX, minY, maxX - minX + 1, maxY - minY + 1);
    }

    /// <summary>
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

                try
                {
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
                finally { Marshal.ReleaseComObject(imageList); }
            }

            return FallbackIcon(path);
        }
        catch
        {
            return FallbackIcon(path);
        }
    }

    /// <summary>
    /// Returns true when the bitmap contains at least one pixel with alpha &gt; 8.
    /// Used to detect fully-transparent HBITMAPs returned by IShellItemImageFactory
    /// for certain file types (e.g. .zip) on some Windows versions.
    /// </summary>
    private static unsafe bool BitmapHasVisiblePixels(Bitmap bmp)
    {
        try
        {
            var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
            var data = bmp.LockBits(rect,
                System.Drawing.Imaging.ImageLockMode.ReadOnly,
                System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            try
            {
                byte* p = (byte*)data.Scan0;
                int total = bmp.Width * bmp.Height;
                for (int i = 0; i < total; i++)
                    if (p[i * 4 + 3] > 8) return true;
                return false;
            }
            finally { bmp.UnlockBits(data); }
        }
        catch { return true; }
    }

    /// <summary>
    /// Returns true when the bitmap's corners are mostly transparent — indicating
    /// a proper shell icon on a transparent background.  Returns false for document
    /// content previews, which have fully-opaque backgrounds (e.g. Office thumbnails).
    /// </summary>
    private static unsafe bool LooksLikeIcon(Bitmap bmp)
    {
        int w = bmp.Width, h = bmp.Height;
        int cr = Math.Max(3, Math.Min(w, h) / 5); // sample ≈20% corner square

        var rect = new Rectangle(0, 0, w, h);
        var data = bmp.LockBits(rect,
            System.Drawing.Imaging.ImageLockMode.ReadOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        try
        {
            byte* p  = (byte*)data.Scan0;
            int total = 0, transparent = 0;
            for (int y = 0; y < cr; y++)
            for (int x = 0; x < cr; x++)
            {
                foreach (int py in (int[])[y, h - 1 - y])
                foreach (int px in (int[])[x, w - 1 - x])
                {
                    total++;
                    if (p[(py * w + px) * 4 + 3] < 64) transparent++;
                }
            }
            // Require at least half the sampled corner pixels to be transparent.
            return total == 0 || transparent * 2 >= total;
        }
        finally { bmp.UnlockBits(data); }
    }


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
