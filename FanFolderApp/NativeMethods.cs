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
    internal const uint SHGFI_SYSICONINDEX   = 0x000004000;
    internal const uint SHGFI_USEFILEATTRIBUTES = 0x000000010;
    internal const uint SHGFI_ICONLOCATION   = 0x000001000;

    // ─── IShellItemImageFactory (modern icon/thumbnail at any size) ────

    internal static readonly Guid IID_IShellItemImageFactory =
        new("BCC18B79-BA16-442F-80C4-8A59C30C463B");

    /// <summary>
    /// Creates a shell item from a file-system path.  Pass
    /// <see cref="IID_IShellItemImageFactory"/> as <paramref name="riid"/> to
    /// get an <see cref="IShellItemImageFactory"/> directly.
    /// </summary>
    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    internal static extern int SHCreateItemFromParsingName(
        string  pszPath,
        IntPtr  pbc,
        ref Guid riid,
        [MarshalAs(UnmanagedType.Interface)] out object? ppv);

    [ComImport]
    [Guid("BCC18B79-BA16-442F-80C4-8A59C30C463B")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IShellItemImageFactory
    {
        [PreserveSig]
        int GetImage(SIZE size, uint flags, out IntPtr phbm);
    }

    /// <summary>Return the icon for the item; never a thumbnail.</summary>
    internal const uint SIIGBF_ICONONLY = 0x04;

    // ─── GetDIBits – read raw pixel bytes from an HBITMAP ─────────────

    [StructLayout(LayoutKind.Sequential)]
    internal struct BITMAPINFOHEADER
    {
        public int   biSize, biWidth, biHeight;
        public short biPlanes, biBitCount;
        public int   biCompression, biSizeImage,
                     biXPelsPerMeter, biYPelsPerMeter,
                     biClrUsed, biClrImportant;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct BITMAPINFO
    {
        public BITMAPINFOHEADER bmiHeader;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1)]
        public int[] bmiColors;
    }

    [DllImport("gdi32.dll")]
    internal static extern int GetDIBits(
        IntPtr hdc, IntPtr hbm, uint uStartScan, uint cScanLines,
        [Out] byte[]? lpvBits, ref BITMAPINFO lpbmi, uint uUsage);

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

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    internal static extern IntPtr FindWindowEx(IntPtr hwndParent, IntPtr hwndChildAfter,
        string lpszClass, string? lpszWindow);

    internal delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool EnumChildWindows(IntPtr hwndParent, EnumWindowsProc lpEnumFunc,
        IntPtr lParam);

    [DllImport("user32.dll")]
    internal static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

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

    // DWM peek / thumbnail control
    internal const int DWMWA_DISALLOW_PEEK              = 11;
    internal const int DWMWA_EXCLUDED_FROM_PEEK         = 12;
    internal const int DWMWA_FORCE_ICONIC_REPRESENTATION = 7;
    internal const int DWMWA_HAS_ICONIC_BITMAP          = 10;

    [DllImport("dwmapi.dll")]
    internal static extern int DwmSetIconicThumbnail(IntPtr hwnd, IntPtr hbmp, uint dwSITFlags);

    // ─── Shell PIDL Functions ──────────────────────────────────────────

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    internal static extern IntPtr ILCreateFromPath(string pszPath);

    [DllImport("shell32.dll")]
    internal static extern void ILFree(IntPtr pidl);

    [DllImport("shell32.dll")]
    internal static extern IntPtr ILClone(IntPtr pidl);

    [DllImport("shell32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool ILRemoveLastID(IntPtr pidl);

    [DllImport("shell32.dll")]
    internal static extern IntPtr ILFindLastID(IntPtr pidl);

    [DllImport("shell32.dll")]
    internal static extern int SHCreateDataObject(
        IntPtr pidlFolder,
        uint cidl,
        [MarshalAs(UnmanagedType.LPArray)] IntPtr[] apidl,
        IntPtr pdtInner,
        ref Guid riid,
        out IntPtr ppv);

    // ─── OLE32 Drag-and-Drop ───────────────────────────────────────────

    [DllImport("ole32.dll")]
    internal static extern int DoDragDrop(
        IntPtr pDataObj,
        IDropSource pDropSource,
        int dwOKEffects,
        out int pdwEffect);

    [ComImport]
    [Guid("00000121-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IDropSource
    {
        [PreserveSig]
        int QueryContinueDrag(
            [MarshalAs(UnmanagedType.Bool)] bool fEscapePressed,
            int grfKeyState);

        [PreserveSig]
        int GiveFeedback(int dwEffect);
    }

    // ─── IDragSourceHelper – custom drag image ─────────────────────────

    [StructLayout(LayoutKind.Sequential)]
    internal struct SIZE
    {
        public int cx;
        public int cy;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct SHDRAGIMAGE
    {
        public SIZE sizeDragImage;
        public Point ptOffset;
        public IntPtr hbmpDragImage;
        public uint crColorKey;
    }

    [ComImport]
    [Guid("DE5BF786-477A-11D2-839D-00C04FD918D0")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IDragSourceHelper
    {
        [PreserveSig]
        int InitializeFromBitmap(ref SHDRAGIMAGE pshdi,
            [MarshalAs(UnmanagedType.Interface)] object pDataObject);

        [PreserveSig]
        int InitializeFromWindow(IntPtr hwnd, ref Point ppt,
            [MarshalAs(UnmanagedType.Interface)] object pDataObject);
    }

    // CLSID for DragDropHelper
    [ComImport]
    [Guid("4657278A-411B-11D2-839A-00C04FD918D0")]
    [ClassInterface(ClassInterfaceType.None)]
    internal class DragDropHelper { }

    [DllImport("gdi32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DeleteObject(IntPtr hObject);

    // ─── GDI DC helpers (for UpdateLayeredWindow) ──────────────────────

    [DllImport("user32.dll")]
    internal static extern IntPtr GetDC(IntPtr hwnd);

    [DllImport("user32.dll")]
    internal static extern int ReleaseDC(IntPtr hwnd, IntPtr hdc);

    [DllImport("gdi32.dll")]
    internal static extern IntPtr CreateCompatibleDC(IntPtr hdc);

    [DllImport("gdi32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DeleteDC(IntPtr hdc);

    [DllImport("gdi32.dll")]
    internal static extern IntPtr SelectObject(IntPtr hdc, IntPtr h);

    // ─── UpdateLayeredWindow (per-pixel alpha transparency) ────────────

    [StructLayout(LayoutKind.Sequential)]
    internal struct BLENDFUNCTION
    {
        public byte BlendOp;              // AC_SRC_OVER = 0
        public byte BlendFlags;           // must be 0
        public byte SourceConstantAlpha;  // 0 = transparent, 255 = opaque
        public byte AlphaFormat;          // AC_SRC_ALPHA = 1 (use per-pixel alpha)
    }

    internal const uint ULW_ALPHA = 2;

    /// <summary>
    /// Updates a layered window's position, size, shape, content, and translucency.
    /// Pass <see cref="IntPtr.Zero"/> for <paramref name="pptDst"/> to keep the current position.
    /// </summary>
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UpdateLayeredWindow(
        IntPtr hwnd,
        IntPtr hdcDst,
        IntPtr pptDst,       // POINT* — IntPtr.Zero keeps current position
        ref SIZE psize,
        IntPtr hdcSrc,
        ref POINT pptSrc,
        uint crKey,
        ref BLENDFUNCTION pblend,
        uint dwFlags);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern int SetWindowRgn(IntPtr hWnd, IntPtr hRgn, [MarshalAs(UnmanagedType.Bool)] bool bRedraw);

    // ─── Message loop primitives (dedicated hook thread) ──────────────

    [StructLayout(LayoutKind.Sequential)]
    internal struct MSG
    {
        public IntPtr hwnd;
        public uint   message;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint   time;
        public POINT  pt;
    }

    [DllImport("user32.dll")]
    internal static extern int GetMessage(out MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool TranslateMessage(ref MSG lpMsg);

    [DllImport("user32.dll")]
    internal static extern IntPtr DispatchMessage(ref MSG lpmsg);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool PostThreadMessage(int idThread, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll")]
    internal static extern int GetCurrentThreadId();

    internal const uint WM_QUIT = 0x0012;

    // ─── Global Mouse Hook (WH_MOUSE_LL) ───────────────────────────────

    internal const int WH_MOUSE_LL     = 14;
    internal const int WM_LBUTTONDOWN  = 0x0201;
    internal const int WM_RBUTTONDOWN  = 0x0204;
    internal const int WM_MBUTTONDOWN  = 0x0207;

    internal delegate IntPtr LowLevelMouseProc(int nCode, IntPtr wParam, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    internal struct MSLLHOOKSTRUCT
    {
        public POINT pt;
        public uint  mouseData;
        public uint  flags;
        public uint  time;
        public IntPtr dwExtraInfo;
    }

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr SetWindowsHookEx(
        int idHook, LowLevelMouseProc lpfn, IntPtr hMod, uint dwThreadId);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UnhookWindowsHookEx(IntPtr hhk);

    [DllImport("user32.dll")]
    internal static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    internal static extern IntPtr GetModuleHandle(string? lpModuleName);

    // ─── Shell Context Menu (IShellFolder / IContextMenu) ─────────────

    internal static readonly Guid IID_IShellFolder = new("000214E6-0000-0000-C000-000000000046");
    internal static readonly Guid IID_IContextMenu  = new("000214E4-0000-0000-C000-000000000046");
    internal static readonly Guid IID_IContextMenu2 = new("000214F4-0000-0000-C000-000000000046");

    /// <summary>Binds to the parent folder of <paramref name="pidl"/> and returns the
    /// relative child PIDL within that folder.  The child PIDL points into the
    /// original <paramref name="pidl"/> allocation — do NOT free it separately.</summary>
    [DllImport("shell32.dll")]
    internal static extern int SHBindToParent(
        IntPtr pidl, ref Guid riid,
        [MarshalAs(UnmanagedType.Interface)] out IShellFolder ppv,
        out IntPtr ppidlLast);

    [ComImport]
    [Guid("000214E6-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IShellFolder
    {
        [PreserveSig] int ParseDisplayName(IntPtr hwnd, IntPtr pbc,
            [MarshalAs(UnmanagedType.LPWStr)] string pszDisplayName,
            out uint pchEaten, out IntPtr ppidl, ref uint pdwAttributes);
        [PreserveSig] int EnumObjects(IntPtr hwnd, uint grfFlags, out IntPtr ppenumIDList);
        [PreserveSig] int BindToObject(IntPtr pidl, IntPtr pbc, ref Guid riid, out IntPtr ppv);
        [PreserveSig] int BindToStorage(IntPtr pidl, IntPtr pbc, ref Guid riid, out IntPtr ppv);
        [PreserveSig] int CompareIDs(IntPtr lParam, IntPtr pidl1, IntPtr pidl2);
        [PreserveSig] int CreateViewObject(IntPtr hwnd, ref Guid riid, out IntPtr ppv);
        [PreserveSig] int GetAttributesOf(uint cidl,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)] IntPtr[] apidl,
            ref uint rgfInOut);
        [PreserveSig] int GetUIObjectOf(IntPtr hwnd, uint cidl,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 1)] IntPtr[] apidl,
            ref Guid riid, IntPtr rgfReserved,
            [MarshalAs(UnmanagedType.Interface)] out object ppv);
    }

    [ComImport]
    [Guid("000214E4-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IContextMenu
    {
        [PreserveSig] int QueryContextMenu(IntPtr hmenu, uint indexMenu,
            int idCmdFirst, int idCmdLast, uint uFlags);
        [PreserveSig] int InvokeCommand(ref CMINVOKECOMMANDINFO lpici);
        [PreserveSig] int GetCommandString(UIntPtr idCmd, uint uType,
            IntPtr pReserved, IntPtr pszName, uint cchMax);
    }

    /// <summary>IContextMenu2 adds HandleMenuMsg needed to populate owner-draw
    /// submenus (e.g. "Send to", "Open with") during TrackPopupMenu's message loop.</summary>
    [ComImport]
    [Guid("000214F4-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IContextMenu2
    {
        [PreserveSig] int QueryContextMenu(IntPtr hmenu, uint indexMenu,
            int idCmdFirst, int idCmdLast, uint uFlags);
        [PreserveSig] int InvokeCommand(ref CMINVOKECOMMANDINFO lpici);
        [PreserveSig] int GetCommandString(UIntPtr idCmd, uint uType,
            IntPtr pReserved, IntPtr pszName, uint cchMax);
        [PreserveSig] int HandleMenuMsg(uint uMsg, IntPtr wParam, IntPtr lParam);
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct CMINVOKECOMMANDINFO
    {
        public int    cbSize;
        public uint   fMask;
        public IntPtr hwnd;
        public IntPtr lpVerb;       // MAKEINTRESOURCE(cmd - idCmdFirst) or verb string
        public IntPtr lpParameters;
        public IntPtr lpDirectory;
        public int    nShow;
        public uint   dwHotKey;
        public IntPtr hIcon;
    }

    internal const uint CMF_EXPLORE   = 0x00000001; // Explorer-style context menu
    internal const uint GCS_VERBW     = 0x00000004; // GetCommandString: return verb as Unicode
    internal const int  ID_CMD_FIRST  = 1;
    internal const int  ID_CMD_LAST   = 0x7FFF;
    internal const int  SW_SHOWNORMAL        = 1;
    internal const int  SW_SHOWMINNOACTIVE   = 7;  // minimize without activating
    internal const int  SW_SHOWNA            = 8;  // show without activating

    // ─── Win32 popup menu ─────────────────────────────────────────────

    [DllImport("user32.dll")]
    internal static extern IntPtr CreatePopupMenu();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DestroyMenu(IntPtr hMenu);

    internal const uint TPM_RETURNCMD   = 0x0100;
    internal const uint TPM_RIGHTBUTTON = 0x0002;

    [DllImport("user32.dll")]
    internal static extern int TrackPopupMenu(
        IntPtr hMenu, uint uFlags,
        int x, int y, int nReserved,
        IntPtr hWnd, IntPtr prcRect);

    // Required before TrackPopupMenu on non-foreground windows
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    internal const uint WM_NULL = 0x0000;

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
