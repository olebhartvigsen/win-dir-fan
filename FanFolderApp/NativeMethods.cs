using System.Runtime.InteropServices;

namespace FanFolderApp;

/// <summary>
/// P/Invoke declarations for Shell32, User32, and Dwmapi.
/// </summary>
internal static class NativeMethods
{
    // ─── Shell Icon Extraction ──────────────────────────────────────────

    [DllImport("shell32.dll", EntryPoint = "#727")]
    internal static extern int SHGetImageList(int iImageList, ref Guid riid, out IImageList ppv);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    internal static extern IntPtr SHGetFileInfo(
        string pszPath,
        uint dwFileAttributes,
        ref SHFILEINFO psfi,
        uint cbSizeFileInfo,
        uint uFlags);

    // IImageList GUID
    internal static readonly Guid IID_IImageList = new("46EB5926-582E-4017-9FDF-E8998DAA0950");

    // SHIL constants – image list sizes
    internal const int SHIL_LARGE = 0x0;       // 32x32
    internal const int SHIL_JUMBO = 0x4;       // 256x256
    internal const int SHIL_EXTRALARGE = 0x2;  // 48x48

    // SHGFI flags
    internal const uint SHGFI_SYSICONINDEX = 0x000004000;
    internal const uint SHGFI_USEFILEATTRIBUTES = 0x000000010;

    // ─── IImageList COM Interface ───────────────────────────────────────

    [ComImport]
    [Guid("46EB5926-582E-4017-9FDF-E8998DAA0950")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IImageList
    {
        [PreserveSig]
        int Add(IntPtr hbmImage, IntPtr hbmMask, ref int pi);
        [PreserveSig]
        int ReplaceIcon(int i, IntPtr hicon, ref int pi);
        [PreserveSig]
        int SetOverlayImage(int iImage, int iOverlay);
        [PreserveSig]
        int Replace(int i, IntPtr hbmImage, IntPtr hbmMask);
        [PreserveSig]
        int AddMasked(IntPtr hbmImage, int crMask, ref int pi);
        [PreserveSig]
        int Draw(ref IMAGELISTDRAWPARAMS pimldp);
        [PreserveSig]
        int Remove(int i);
        [PreserveSig]
        int GetIcon(int i, int flags, ref IntPtr picon);
        [PreserveSig]
        int GetImageInfo(int i, ref IMAGEINFO pImageInfo);
        [PreserveSig]
        int Copy(int iDst, IImageList punkSrc, int iSrc, int uFlags);
        [PreserveSig]
        int Merge(int i1, IImageList punk2, int i2, int dx, int dy, ref Guid riid, ref IntPtr ppv);
        [PreserveSig]
        int Clone(ref Guid riid, ref IntPtr ppv);
        [PreserveSig]
        int GetImageRect(int i, ref RECT prc);
        [PreserveSig]
        int GetIconSize(ref int cx, ref int cy);
        [PreserveSig]
        int SetIconSize(int cx, int cy);
        [PreserveSig]
        int GetImageCount(ref int pi);
        [PreserveSig]
        int SetImageCount(int uNewCount);
        [PreserveSig]
        int SetBkColor(int clrBk, ref int pclr);
        [PreserveSig]
        int GetBkColor(ref int pclr);
        [PreserveSig]
        int BeginDrag(int iTrack, int dxHotspot, int dyHotspot);
        [PreserveSig]
        int EndDrag();
        [PreserveSig]
        int DragEnter(IntPtr hwndLock, int x, int y);
        [PreserveSig]
        int DragLeave(IntPtr hwndLock);
        [PreserveSig]
        int DragMove(int x, int y);
        [PreserveSig]
        int SetDragCursorImage(ref IImageList punk, int iDrag, int dxHotspot, int dyHotspot);
        [PreserveSig]
        int DragShowNolock(int fShow);
        [PreserveSig]
        int GetDragImage(ref POINT ppt, ref POINT pptHotspot, ref Guid riid, ref IntPtr ppv);
        [PreserveSig]
        int GetItemFlags(int i, ref int dwFlags);
        [PreserveSig]
        int GetOverlayImage(int iOverlay, ref int piIndex);
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct IMAGELISTDRAWPARAMS
    {
        public int cbSize;
        public IntPtr himl;
        public int i, hdcDst, x, y, cx, cy, xBitmap, yBitmap;
        public int rgbBk, rgbFg, fStyle, dwRop;
        public int fState, Frame, crEffect;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct IMAGEINFO
    {
        public IntPtr hbmImage;
        public IntPtr hbmMask;
        public int Unused1, Unused2;
        public RECT rcImage;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct POINT
    {
        public int X, Y;
    }

    // ─── Shell File Info ────────────────────────────────────────────────

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct SHFILEINFO
    {
        public IntPtr hIcon;
        public int iIcon;
        public uint dwAttributes;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szDisplayName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
        public string szTypeName;
    }

    // ─── User32 – Taskbar Detection ────────────────────────────────────

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr FindWindow(string lpClassName, string? lpWindowName);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DestroyIcon(IntPtr hIcon);

    [DllImport("user32.dll")]
    internal static extern int GetSystemMetrics(int nIndex);

    [DllImport("user32.dll")]
    internal static extern IntPtr MonitorFromPoint(POINT pt, uint dwFlags);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetCursorPos(out POINT lpPoint);

    internal const int SM_CXSCREEN = 0;
    internal const int SM_CYSCREEN = 1;
    internal const uint MONITOR_DEFAULTTONEAREST = 2;

    // ─── Shcore – DPI ──────────────────────────────────────────────────

    [DllImport("shcore.dll")]
    internal static extern int GetDpiForMonitor(IntPtr hmonitor, int dpiType, out uint dpiX, out uint dpiY);

    // ─── APPBARDATA – Taskbar Info ──────────────────────────────────────

    [DllImport("shell32.dll")]
    internal static extern uint SHAppBarMessage(uint dwMessage, ref APPBARDATA pData);

    internal const uint ABM_GETTASKBARPOS = 0x00000005;

    [StructLayout(LayoutKind.Sequential)]
    internal struct APPBARDATA
    {
        public int cbSize;
        public IntPtr hWnd;
        public uint uCallbackMessage;
        public uint uEdge;
        public RECT rc;
        public int lParam;
    }

    internal const uint ABE_LEFT = 0;
    internal const uint ABE_TOP = 1;
    internal const uint ABE_RIGHT = 2;
    internal const uint ABE_BOTTOM = 3;

    // ─── Common Structs ────────────────────────────────────────────────

    [StructLayout(LayoutKind.Sequential)]
    internal struct RECT
    {
        public int Left, Top, Right, Bottom;
    }

    // ─── Dwmapi – Drop Shadow ──────────────────────────────────────────

    [DllImport("dwmapi.dll")]
    internal static extern int DwmSetWindowAttribute(
        IntPtr hwnd,
        int dwAttribute,
        ref int pvAttribute,
        int cbAttribute);

    // DWMWA_NCRENDERING_POLICY = 2, DWMNCRP_ENABLED = 2
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    // DWMWA_WINDOW_CORNER_PREFERENCE = 33
    // DWMWA_BORDER_COLOR = 34

    internal const int DWMWA_NCRENDERING_POLICY = 2;
    internal const int DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
    internal const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    internal const int DWMWCP_ROUND = 2;
}
