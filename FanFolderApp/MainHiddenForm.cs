using System.Runtime.InteropServices;

namespace FanFolderApp;

/// <summary>
/// Invisible main form whose sole purpose is to keep an icon in the
/// Windows taskbar. Clicking the taskbar icon activates this form,
/// which toggles the fan popup.
/// </summary>
internal sealed class MainHiddenForm : Form
{
    private readonly string _folderPath;
    private FanForm? _fanForm;
    private FanForm? _prewarmFanForm;
    private bool _suppressNextActivation;
    private DateTime _lastFanCloseTime = DateTime.MinValue;
    private Icon? _stackIcon;
    private Icon? _arrowIcon;

    // ─── Global mouse hook — closes fan on outside click ──────────────
    // The hook runs on a dedicated background thread so its callback is never
    // delayed by UI-thread rendering (UpdateLayeredWindow).  Blocking the hook
    // callback on the UI thread was the root cause of cursor jitter.
    private NativeMethods.LowLevelMouseProc? _mouseHookProc; // keep delegate alive (prevent GC)
    private IntPtr   _mouseHook     = IntPtr.Zero;
    private Thread?  _hookThread;
    private volatile int _hookThreadId;

    // Pre-warmed file listing — refreshed in the background after every open
    private volatile List<FileSystemInfo>? _cachedItems;

    public MainHiddenForm(string folderPath)
    {
        _folderPath = folderPath;

        // Keep taskbar icon, but make form invisible
        Text = "Fan Folder";
        ShowInTaskbar = true;
        Opacity = 0;
        WindowState = FormWindowState.Minimized;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MinimizeBox = false;
        MaximizeBox = false;
        Size = new Size(1, 1);
        Location = new Point(-10000, -10000); // off-screen as backup

        // Set a folder-style icon (use shell icon of the folder itself)
        SetTaskbarIcon();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);

        int trueVal = 1;
        // Suppress Aero Peek
        NativeMethods.DwmSetWindowAttribute(Handle, NativeMethods.DWMWA_DISALLOW_PEEK,     ref trueVal, sizeof(int));
        NativeMethods.DwmSetWindowAttribute(Handle, NativeMethods.DWMWA_EXCLUDED_FROM_PEEK, ref trueVal, sizeof(int));
        // Tell DWM we supply a custom iconic thumbnail — prevents it from showing
        // the real (tiny, invisible) window as the hover preview.
        NativeMethods.DwmSetWindowAttribute(Handle, NativeMethods.DWMWA_HAS_ICONIC_BITMAP,           ref trueVal, sizeof(int));
        NativeMethods.DwmSetWindowAttribute(Handle, NativeMethods.DWMWA_FORCE_ICONIC_REPRESENTATION, ref trueVal, sizeof(int));

        StartPrewarm();
    }

    /// <summary>
    /// Fetches the recent-items list on a background thread and stores it in
    /// <see cref="_cachedItems"/> so <see cref="OpenFan"/> can use it instantly.
    /// </summary>
    private void StartPrewarm()
    {
        var path = _folderPath;
        Task.Run(() => FileService.GetRecentItems(path))
            .ContinueWith(t =>
            {
                if (!t.IsFaulted)
                {
                    _cachedItems = t.Result;
                    // Pre-build FanForm + force HWND creation on the UI thread so
                    // the next OpenFan() only needs Show() — not CreateWindowEx.
                    if (IsHandleCreated && !IsDisposed)
                        BeginInvoke(() => BuildPrewarmForm(t.Result));
                }
            }, TaskScheduler.Default);
    }

    private void BuildPrewarmForm(IReadOnlyList<FileSystemInfo> items)
    {
        _prewarmFanForm?.Dispose();
        _prewarmFanForm = new FanForm(_folderPath, items);
        _ = _prewarmFanForm.Handle; // force HWND creation now, not on click
    }

    // ─── Taskbar Icon ────────────────────────────────────────

    private void SetTaskbarIcon()
    {
        _stackIcon = CreateStackIcon(256);
        _arrowIcon = CreateArrowIcon(256);
        Icon = _stackIcon;
    }

    /// <summary>
    /// Draws three document sheets fanning out from a bottom-centre pivot —
    /// mimicking the macOS Dock "fan" stack.  Blue left, white centre (front),
    /// amber right.  High-contrast design with thick borders and bold fills.
    /// </summary>
    private static Icon CreateStackIcon(int size)
    {
        using var bmp = new Bitmap(size, size, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(bmp);
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        g.Clear(Color.Transparent);

        float cx     = size * 0.50f;
        float pivotY = size * 0.86f;   // near the bottom
        float dW     = size * 0.46f;
        float dH     = size * 0.75f;
        float cr     = size * 0.04f;
        float fold   = size * 0.11f;

        // Left (sky blue) — behind centre
        DrawFanDoc(g, cx, pivotY, dW, dH * 0.96f, cr, fold, size,
            -22f,
            Color.FromArgb(60, 165, 252),
            Color.FromArgb(15, 75, 170),
            Color.FromArgb(120, 175, 235));

        // Right (amber) — behind centre
        DrawFanDoc(g, cx, pivotY, dW, dH * 0.96f, cr, fold, size,
            22f,
            Color.FromArgb(255, 198, 40),
            Color.FromArgb(160, 96, 5),
            Color.FromArgb(205, 162, 70));

        // Centre (white) — front
        DrawFanDoc(g, cx, pivotY, dW * 1.06f, dH, cr, fold, size,
            0f,
            Color.White,
            Color.FromArgb(35, 40, 58),
            Color.FromArgb(162, 165, 180));

        IntPtr handle = bmp.GetHicon();
        Icon owned = (Icon)Icon.FromHandle(handle).Clone();
        NativeMethods.DestroyIcon(handle);
        return owned;
    }

    private static void DrawFanDoc(Graphics g,
        float pivotX, float pivotY, float docW, float docH,
        float cr, float fold, int size,
        float angle, Color fillColor, Color borderColor, Color lineColor)
    {
        using var mx = new System.Drawing.Drawing2D.Matrix();
        mx.RotateAt(angle, new PointF(pivotX, pivotY));
        g.Transform = mx;

        float rx   = pivotX - docW / 2f;
        float ry   = pivotY - docH;
        var rect   = new RectangleF(rx, ry, docW, docH);

        // Drop shadow
        using var sp = RoundedDocPath(rect, cr, fold);
        using var sh = new SolidBrush(Color.FromArgb(90, 0, 0, 0));
        g.TranslateTransform(3, 4); g.FillPath(sh, sp); g.TranslateTransform(-3, -4);

        // Fill + border
        using var dp = RoundedDocPath(rect, cr, fold);
        using var fb = new SolidBrush(fillColor);
        g.FillPath(fb, dp);
        using var bp = new Pen(borderColor, size * 0.030f);
        g.DrawPath(bp, dp);

        // Content lines
        using var lp = new Pen(lineColor, size * 0.026f);
        float lx = rect.X + rect.Width * 0.14f;
        float lw = rect.Width * 0.58f;    // leave fold corner clear
        for (int i = 0; i < 3; i++)
        {
            float ly = rect.Y + rect.Height * (0.36f + i * 0.14f);
            g.DrawLine(lp, lx, ly, lx + lw, ly);
        }

        g.ResetTransform();
    }

    /// <summary>
    /// Creates a document-shaped path with rounded corners and a folded top-right corner.
    /// </summary>
    private static System.Drawing.Drawing2D.GraphicsPath RoundedDocPath(
        RectangleF rect, float cornerRadius, float foldSize)
    {
        var p = new System.Drawing.Drawing2D.GraphicsPath();
        float x = rect.X, y = rect.Y, w = rect.Width, h = rect.Height;
        float cr = cornerRadius;

        // Start at top-left corner
        p.AddArc(x, y, cr, cr, 180, 90);
        // Top edge to fold
        p.AddLine(x + cr, y, x + w - foldSize, y);
        // Fold diagonal
        p.AddLine(x + w - foldSize, y, x + w, y + foldSize);
        // Right edge
        p.AddLine(x + w, y + foldSize, x + w, y + h - cr);
        // Bottom-right corner
        p.AddArc(x + w - cr, y + h - cr, cr, cr, 0, 90);
        // Bottom edge
        p.AddLine(x + w - cr, y + h, x + cr, y + h);
        // Bottom-left corner
        p.AddArc(x, y + h - cr, cr, cr, 90, 90);
        p.CloseFigure();
        return p;
    }

    /// <summary>
    /// Draws a macOS-style "active" icon: a downward arrowhead inside a rounded box.
    /// </summary>
    private static Icon CreateArrowIcon(int size)
    {
        using var bmp = new Bitmap(size, size, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(bmp);
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        g.Clear(Color.Transparent);

        float pad = size * 0.08f;
        float boxSize = size - pad * 2;
        float cr = size * 0.14f; // corner radius

        // Rounded box background
        using var boxPath = new System.Drawing.Drawing2D.GraphicsPath();
        var rect = new RectangleF(pad, pad, boxSize, boxSize);
        boxPath.AddArc(rect.X, rect.Y, cr, cr, 180, 90);
        boxPath.AddArc(rect.Right - cr, rect.Y, cr, cr, 270, 90);
        boxPath.AddArc(rect.Right - cr, rect.Bottom - cr, cr, cr, 0, 90);
        boxPath.AddArc(rect.X, rect.Bottom - cr, cr, cr, 90, 90);
        boxPath.CloseFigure();

        using var boxFill = new System.Drawing.Drawing2D.LinearGradientBrush(
            new PointF(pad, pad), new PointF(pad, pad + boxSize),
            Color.FromArgb(110, 170, 240), Color.FromArgb(60, 120, 200));
        g.FillPath(boxFill, boxPath);

        using var boxPen = new Pen(Color.FromArgb(45, 90, 165), size * 0.016f);
        g.DrawPath(boxPen, boxPath);

        // Downward arrowhead (chevron)
        float cx = size / 2f;
        float cy = size / 2f + size * 0.02f;
        float aw = boxSize * 0.38f; // half-width of arrow
        float ah = boxSize * 0.22f; // height of arrow

        using var arrowPen = new Pen(Color.White, size * 0.07f)
        {
            StartCap = System.Drawing.Drawing2D.LineCap.Round,
            EndCap = System.Drawing.Drawing2D.LineCap.Round,
            LineJoin = System.Drawing.Drawing2D.LineJoin.Round
        };
        g.DrawLine(arrowPen, cx - aw, cy - ah, cx, cy + ah);
        g.DrawLine(arrowPen, cx + aw, cy - ah, cx, cy + ah);

        IntPtr handle = bmp.GetHicon();
        Icon owned = (Icon)Icon.FromHandle(handle).Clone();
        NativeMethods.DestroyIcon(handle);
        return owned;
    }

    // ─── Activation / Toggle ─────────────────────────────────

    protected override void WndProc(ref Message m)
    {
        const int WM_SYSCOMMAND              = 0x0112;
        const int SC_RESTORE                 = 0xF120;
        const int WM_DWMSENDICONICTHUMBNAIL  = 0x0323;

        if (m.Msg == WM_SYSCOMMAND && (m.WParam.ToInt32() & 0xFFF0) == SC_RESTORE)
        {
            bool fanIsOpen = _fanForm != null && _fanForm.Visible;
            // Toggle the fan FIRST so it is already shown before base.WndProc
            // activates MainHiddenForm.  The 250 ms grace period in the Deactivate
            // handler absorbs the resulting spurious Deactivate event.
            if (fanIsOpen || IsCursorNearTaskbar())
                ToggleFan();
            // Call base.WndProc so Shell registers a successful restore and does
            // NOT re-send SC_RESTORE 1-2 s later.
            base.WndProc(ref m);
            WindowState = FormWindowState.Minimized;
            return;
        }

        // DWM is asking for our custom hover-preview thumbnail.
        if (m.Msg == WM_DWMSENDICONICTHUMBNAIL)
        {
            int maxW = (m.LParam.ToInt32() >> 16) & 0xFFFF;
            int maxH =  m.LParam.ToInt32()        & 0xFFFF;
            ProvideIconicThumbnail(maxW, maxH);
            m.Result = IntPtr.Zero;
            return;
        }

        base.WndProc(ref m);
    }

    /// <summary>
    /// Renders the stack icon at the size requested by DWM and hands the
    /// HBITMAP to <c>DwmSetIconicThumbnail</c> so the taskbar hover popup
    /// shows a clean preview instead of the tiny invisible host window.
    /// </summary>
    private void ProvideIconicThumbnail(int maxW, int maxH)
    {
        int sz = Math.Min(Math.Min(maxW, maxH), 256);
        if (sz <= 0) sz = 128;

        // Render onto a premultiplied-alpha bitmap (required by DWM)
        using var bmp = new Bitmap(sz, sz, System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        using (var g = System.Drawing.Graphics.FromImage(bmp))
        {
            g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
            g.Clear(Color.Transparent);

            // Faint rounded-rect background so the icon is visible on any colour popup
            float pad = sz * 0.06f;
            using var bgBrush = new SolidBrush(Color.FromArgb(30, 255, 255, 255));
            g.FillRectangle(bgBrush, pad, pad, sz - pad * 2, sz - pad * 2);

            // Reuse the same drawing logic as the taskbar icon
            float cx = sz * 0.50f, pivotY = sz * 0.86f;
            float dW = sz * 0.46f, dH = sz * 0.75f;
            float cr = sz * 0.04f, fold = sz * 0.11f;

            DrawFanDoc(g, cx, pivotY, dW, dH * 0.96f, cr, fold, sz, -22f,
                Color.FromArgb(60, 165, 252), Color.FromArgb(15, 75, 170), Color.FromArgb(120, 175, 235));
            DrawFanDoc(g, cx, pivotY, dW, dH * 0.96f, cr, fold, sz, 22f,
                Color.FromArgb(255, 198, 40), Color.FromArgb(160, 96, 5), Color.FromArgb(205, 162, 70));
            DrawFanDoc(g, cx, pivotY, dW * 1.06f, dH, cr, fold, sz, 0f,
                Color.White, Color.FromArgb(35, 40, 58), Color.FromArgb(162, 165, 180));
        }

        IntPtr hBmp = bmp.GetHbitmap(Color.FromArgb(0));
        try   { NativeMethods.DwmSetIconicThumbnail(Handle, hBmp, 0); }
        finally { NativeMethods.DeleteObject(hBmp); }
    }

    /// <summary>
    /// Returns true when the cursor is within or very close to the taskbar rect.
    /// Used to distinguish a real taskbar click from an Alt+Tab SC_RESTORE.
    /// </summary>
    private static bool IsCursorNearTaskbar()
    {
        NativeMethods.GetCursorPos(out var cursor);
        var abd = new NativeMethods.APPBARDATA
        {
            cbSize = Marshal.SizeOf<NativeMethods.APPBARDATA>()
        };
        NativeMethods.SHAppBarMessage(NativeMethods.ABM_GETTASKBARPOS, ref abd);
        var tb = abd.rc;
        const int margin = 48;
        return cursor.X >= tb.Left - margin && cursor.X <= tb.Right  + margin
            && cursor.Y >= tb.Top  - margin && cursor.Y <= tb.Bottom + margin;
    }

    protected override void OnActivated(EventArgs e)    {
        base.OnActivated(e);

        if (_suppressNextActivation)
        {
            _suppressNextActivation = false;
            return;
        }

        // Ensure form stays invisible and minimized
        WindowState = FormWindowState.Minimized;
        Opacity = 0;
    }

    // ─── Fan Toggle ──────────────────────────────────────────

    private void ToggleFan()
    {
        if (_fanForm != null && _fanForm.Visible)
        {
            CloseFan();
        }
        else
        {
            // If the fan was just closed (e.g. by Deactivate from this
            // same taskbar click), don't reopen it.
            if ((DateTime.UtcNow - _lastFanCloseTime).TotalMilliseconds < 500)
                return;
            OpenFan();
        }
    }

    private void OpenFan()
    {
        CloseFan();

        // Use the pre-warmed form if ready (HWND already created — much faster).
        var form = _prewarmFanForm;
        _prewarmFanForm = null;

        if (form == null || form.IsDisposed)
        {
            // Fallback: pre-warm wasn't ready yet.
            var items = _cachedItems;
            _cachedItems = null;
            form = new FanForm(_folderPath, items);
        }

        form.Reposition(); // snap to current cursor — fast (no arc recalc)
        _fanForm = form;
        _fanForm.FormClosed += (_, _) => { _fanForm = null; Icon = _stackIcon; StartPrewarm(); };

        // Record when we opened the fan so the Deactivate handler can ignore the
        // spurious event that fires when base.WndProc(SC_RESTORE) briefly activates
        // MainHiddenForm (which in turn deactivates the FanForm).  Any Deactivate
        // arriving within 250 ms of Show() is treated as noise and suppressed.
        var openedAt = DateTime.UtcNow;
        _fanForm.Deactivate += (_, _) =>
        {
            if ((DateTime.UtcNow - openedAt).TotalMilliseconds < 250) return;
            _suppressNextActivation = true;
            CloseFan();
        };
        _fanForm.Show();
        Icon = _arrowIcon;
        InstallMouseHook();
        StartPrewarm();
    }

    private void CloseFan()
    {
        UninstallMouseHook();
        // Stamp the close time FIRST so any SC_RESTORE arriving during Close()
        // (WinForms processes queued messages synchronously inside Close())
        // will see a recent timestamp and skip re-opening the fan.
        _lastFanCloseTime = DateTime.UtcNow;

        if (_fanForm != null && !_fanForm.IsDisposed)
        {
            // Null the field before calling Close() so re-entrant calls
            // from the Deactivate event (fired inside Close()) are no-ops.
            var form = _fanForm;
            _fanForm = null;
            form.Close();
        }
        Icon = _stackIcon;
    }

    // ─── Global mouse hook ────────────────────────────────────────────

    private void InstallMouseHook()
    {
        if (_hookThread is { IsAlive: true }) return;
        _hookThread = new Thread(HookThreadProc) { IsBackground = true, Name = "MouseHookThread" };
        _hookThread.Start();
    }

    /// <summary>
    /// Runs a minimal Win32 message loop on a dedicated background thread so the
    /// WH_MOUSE_LL callback is never blocked by UI-thread rendering work.
    /// </summary>
    private void HookThreadProc()
    {
        _hookThreadId  = NativeMethods.GetCurrentThreadId();
        _mouseHookProc = MouseHookCallback;
        using var module = System.Diagnostics.Process.GetCurrentProcess().MainModule!;
        _mouseHook = NativeMethods.SetWindowsHookEx(
            NativeMethods.WH_MOUSE_LL, _mouseHookProc,
            NativeMethods.GetModuleHandle(module.ModuleName), 0);

        while (NativeMethods.GetMessage(out var msg, IntPtr.Zero, 0, 0) > 0)
        {
            NativeMethods.TranslateMessage(ref msg);
            NativeMethods.DispatchMessage(ref msg);
        }

        if (_mouseHook != IntPtr.Zero)
        {
            NativeMethods.UnhookWindowsHookEx(_mouseHook);
            _mouseHook = IntPtr.Zero;
        }
        _mouseHookProc = null;
        _hookThreadId  = 0;
    }

    private void UninstallMouseHook()
    {
        int tid = _hookThreadId;
        if (tid != 0)
            NativeMethods.PostThreadMessage(tid, NativeMethods.WM_QUIT, IntPtr.Zero, IntPtr.Zero);
        _hookThread = null;
    }

    private IntPtr MouseHookCallback(int nCode, IntPtr wParam, IntPtr lParam)
    {
        if (nCode >= 0)
        {
            int msg = wParam.ToInt32();
            if (msg == NativeMethods.WM_LBUTTONDOWN ||
                msg == NativeMethods.WM_RBUTTONDOWN ||
                msg == NativeMethods.WM_MBUTTONDOWN)
            {
                var fan = _fanForm;
                if (fan != null && !fan.IsDisposed && fan.Visible)
                {
                    var hs       = Marshal.PtrToStructure<NativeMethods.MSLLHOOKSTRUCT>(lParam);
                    var clickPt  = new Point(hs.pt.X, hs.pt.Y);
                    if (!fan.Bounds.Contains(clickPt) && !fan.IsContextMenuOpen)
                        BeginInvoke(CloseFan); // marshal back to UI thread
                }
            }
        }
        return NativeMethods.CallNextHookEx(_mouseHook, nCode, wParam, lParam);
    }

    // ─── Cleanup ─────────────────────────────────────────────

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            CloseFan(); // also calls UninstallMouseHook
            _prewarmFanForm?.Dispose();
            _stackIcon?.Dispose();
            _arrowIcon?.Dispose();
        }
        base.Dispose(disposing);
    }
}
