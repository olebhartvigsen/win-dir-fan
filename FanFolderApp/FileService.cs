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
    /// Extracts a shell icon for the given path using the native IImageList
    /// obtained via SHGetImageList.  Tries SHIL_JUMBO (256×256) first, then
    /// SHIL_EXTRALARGE (48×48), then SHIL_LARGE (32×32) as fallback.
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

            // Try progressively smaller image lists until one works
            Guid iidImageList = NativeMethods.IID_IImageList;
            int result = NativeMethods.SHGetImageList(
                NativeMethods.SHIL_JUMBO, ref iidImageList, out var imageList);

            if (result != 0 || imageList == null)
            {
                iidImageList = NativeMethods.IID_IImageList;
                result = NativeMethods.SHGetImageList(
                    NativeMethods.SHIL_EXTRALARGE, ref iidImageList, out imageList);
            }

            if (result != 0 || imageList == null)
            {
                iidImageList = NativeMethods.IID_IImageList;
                result = NativeMethods.SHGetImageList(
                    NativeMethods.SHIL_LARGE, ref iidImageList, out imageList);
            }

            if (result != 0 || imageList == null)
                return FallbackIcon(path);

            IntPtr hIcon = IntPtr.Zero;
            const int ILD_TRANSPARENT = 0x00000001;
            result = imageList.GetIcon(iconIndex, ILD_TRANSPARENT, ref hIcon);

            if (result != 0 || hIcon == IntPtr.Zero)
                return FallbackIcon(path);

            // Clone the icon so we can immediately release the native handle
            Icon tempIcon = Icon.FromHandle(hIcon);
            Icon cloned = (Icon)tempIcon.Clone();
            NativeMethods.DestroyIcon(hIcon);

            return cloned;
        }
        catch
        {
            return FallbackIcon(path);
        }
    }

    private static Icon? FallbackIcon(string path)
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
