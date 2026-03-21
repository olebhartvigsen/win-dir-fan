using System.Diagnostics;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Runtime.InteropServices;

namespace FanFolderApp;

/// <summary>
/// Transparent, borderless, top-most fan popup that displays the 15 most
/// recent items along a gentle arc emanating from the taskbar — replicating
/// the macOS Dock "Fan" folder style.  Icons float on a fully transparent
/// background with a text label to the left.  Icon size adapts to the screen.
/// </summary>
internal sealed class FanForm : Form
{
    // ─── Layout ──────────────────────────────────────────────
    private const int MaxItems = 15;
    private const int FormMargin = 20;
    private const int LabelGap = 6;         // gap between label and icon

    // ─── Arc Parameters (gentle curve) ───────────────────────
    private const float StartDistance = 60f;
    private const float DistanceStep = 1.15f;  // multiplier (was additive px)
    private const float MaxArcSpreadDeg = 22f;  // very gentle
    private const float ArcSpreadPerItem = 1.5f;

    // ─── Colours ─────────────────────────────────────────────
    private static readonly Color TextNormal = Color.White;
    private static readonly Color TextShadow = Color.FromArgb(200, 0, 0, 0);
    private static readonly Color HoverTint  = Color.FromArgb(60, 255, 255, 255);

    // ─── Animation ──────────────────────────────────────────────
    private const float HoverScaleMax = 1.4f;
    private const float AnimSpeed = 0.53f;  // progress per tick (≈ 2 ticks / ~30 ms to full)
    private static readonly Color TextHover = Color.FromArgb(255, 255, 230, 120); // warm gold

    // ─── Fade-in / slide-up ─────────────────────────────────────
    private const float FadeDurationMs = 200f;
    private const int SlideDistance = 24; // pixels to slide up
    private System.Windows.Forms.Timer? _fadeTimer;
    private float _fadeProgress;           // 0 → 1
    private int _targetTop;                // final Y position

    // ─── State ───────────────────────────────────────────────────
    private readonly List<FanItem> _items = [];
    private readonly string _folderPath;
    private RectangleF[] _hitRects = [];   // icon hit area per item
    private PointF[] _iconPositions = [];  // top-left of each icon
    private int _hoveredIndex = -1;
    private float[] _animProgress = [];    // 0..1 per item (eased)
    private System.Windows.Forms.Timer? _animTimer;
    private Point _mouseDownPos;           // for drag detection
    private int _mouseDownIndex = -1;      // item index at mouse-down
    private bool _dragging;                // true while DoDragDrop is active
    private int _iconSize;                 // adaptive
    private float _itemSpacing;            // adaptive
    private float _fontSize;               // adaptive
    private int _maxStackHeight;           // 75 % of screen height

    // ─── Rendering Cache ─────────────────────────────────────────────
    private Bitmap? _offscreenBmp;
    private byte    _currentAlpha;   // 0–255; drives UpdateLayeredWindow fade
    private Font?   _font;
    private StringFormat? _sf;
    private int[] _drawOrder = [];

    // ─── Inner Model ─────────────────────────────────────────
    private sealed class FanItem
    {
        public required string Name { get; init; }
        public required string FullPath { get; init; }
        public required bool IsDirectory { get; init; }
        public bool IsArrow { get; init; }
        public Icon? Icon { get; set; }
        public Bitmap? Bmp { get; set; }   // cached bitmap from Icon, avoids ToBitmap() per frame
    }

    // ═════════════════════════════════════════════════════════
    //  Construction
    // ═════════════════════════════════════════════════════════

    public FanForm(string folderPath)
    {
        _folderPath = folderPath;

        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        // WS_EX_LAYERED is set via CreateParams; per-pixel alpha via UpdateLayeredWindow.
        // Do NOT set TransparencyKey, BackColor=Magenta, or Opacity here — they conflict.
        KeyPreview = true;

        ComputeAdaptiveSizes();
        LoadItems();
        CalculateArcLayout();
        _ = LoadIconsAsync(); // fire-and-forget: icons fill in as they load
        InitAnimation();
        _sf = new StringFormat
        {
            Alignment = StringAlignment.Far,
            LineAlignment = StringAlignment.Center,
            Trimming = StringTrimming.EllipsisCharacter,
            FormatFlags = StringFormatFlags.NoWrap
        };
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        // Capture final position and offset downward for slide-up
        _targetTop = Top;
        Top += SlideDistance;
        _currentAlpha = 0;
        DrawToLayeredWindow();  // establish transparent layered surface before fade starts
        _fadeProgress = 0f;
        _fadeTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _fadeTimer.Tick += OnFadeTick;
        _fadeTimer.Start();
    }

    private void OnFadeTick(object? sender, EventArgs e)
    {
        _fadeProgress += 16f / FadeDurationMs;
        if (_fadeProgress >= 1f)
        {
            _fadeProgress = 1f;
            _fadeTimer?.Stop();
            _fadeTimer?.Dispose();
            _fadeTimer = null;
        }

        // Ease-out cubic: fast start, smooth deceleration
        float t = 1f - MathF.Pow(1f - _fadeProgress, 3f);

        _currentAlpha = (byte)Math.Clamp((int)(t * 255f), 0, 255);
        Top = _targetTop + (int)(SlideDistance * (1f - t));
        DrawToLayeredWindow();
    }

    private void InitAnimation()
    {
        _animProgress = new float[_items.Count];
        _animTimer = new System.Windows.Forms.Timer { Interval = 16 }; // ~60 fps
        _animTimer.Tick += OnAnimTick;
        _animTimer.Start();
    }

    private void OnAnimTick(object? sender, EventArgs e)
    {
        bool needsRepaint = false;
        bool allSettled   = true;

        for (int i = 0; i < _animProgress.Length; i++)
        {
            float target = (i == _hoveredIndex) ? 1f : 0f;
            float cur = _animProgress[i];

            if (MathF.Abs(cur - target) < 0.005f)
            {
                if (cur != target) { _animProgress[i] = target; needsRepaint = true; }
                continue;
            }

            allSettled = false;
            // Move linearly; easing is applied when reading the value
            _animProgress[i] += (target > cur) ? AnimSpeed : -AnimSpeed;
            _animProgress[i] = Math.Clamp(_animProgress[i], 0f, 1f);
            needsRepaint = true;
        }

        if (needsRepaint) DrawToLayeredWindow();

        // Stop timer when all animations have settled; restarts on mouse enter/move.
        if (allSettled) _animTimer?.Stop();
    }

    /// <summary>Ease-out cubic: fast start, smooth deceleration — no initial delay.</summary>
    private static float EaseInOut(float t)
    {
        return 1f - MathF.Pow(1f - t, 3f);
    }

    // ═════════════════════════════════════════════════════════
    //  Adaptive Sizing
    // ═════════════════════════════════════════════════════════

    private void ComputeAdaptiveSizes()
    {
        NativeMethods.GetCursorPos(out var cursor);
        var screen = Screen.FromPoint(new Point(cursor.X, cursor.Y));
        int screenH = screen.Bounds.Height;

        _maxStackHeight = (int)(screenH * 0.75f);

        // Icon size scales with screen height:
        //   1080p → 56px,  1440p → 72px,  2160p (4K) → 96px
        _iconSize = Math.Clamp(screenH / 19, 48, 128);
        _itemSpacing = _iconSize * 1.3f;
        _fontSize = _iconSize / 5.5f;
    }

    // ═════════════════════════════════════════════════════════
    //  Data Loading
    // ═════════════════════════════════════════════════════════

    private void LoadItems()
    {
        if (!Directory.Exists(_folderPath))
        {
            _items.Add(new FanItem
            {
                Name = $"Path not found: {_folderPath}",
                FullPath = string.Empty,
                IsDirectory = false
            });
            return;
        }

        var entries = FileService.GetRecentItems(_folderPath);

        foreach (var entry in entries)
        {
            _items.Add(new FanItem
            {
                Name = entry.Name,
                FullPath = entry.FullName,
                IsDirectory = entry is DirectoryInfo,
                // Icons are loaded asynchronously via LoadIconsAsync()
            });
        }

        if (_items.Count == 0)
        {
            _items.Add(new FanItem
            {
                Name = "(empty folder)",
                FullPath = string.Empty,
                IsDirectory = false
            });
        }

        // Top-most item: an arrow that opens the folder (like macOS fan)
        _items.Add(new FanItem
        {
            Name = "Open in Explorer",
            FullPath = _folderPath,
            IsDirectory = true,
            IsArrow = true
        });
    }

    /// <summary>
    /// Loads shell icons for all non-arrow items in parallel on background threads.
    /// Each icon is applied to its item and redraws the window as it arrives,
    /// so the form appears immediately and icons fill in within milliseconds.
    /// </summary>
    private async Task LoadIconsAsync()
    {
        var tasks = _items
            .Where(item => !item.IsArrow && !string.IsNullOrEmpty(item.FullPath))
            .Select(item =>
            {
                var path = item.FullPath;
                return Task.Run(() => (item, icon: FileService.GetShellIcon(path)));
            })
            .ToList();

        foreach (var loadTask in tasks)
        {
            var (item, icon) = await loadTask;
            if (IsDisposed) return;
            item.Icon = icon;
            item.Bmp  = icon?.ToBitmap();
            DrawToLayeredWindow();
        }
    }

    // ═════════════════════════════════════════════════════════
    //  Arc Layout
    // ═════════════════════════════════════════════════════════

    private enum TaskbarEdge { Bottom, Top, Left, Right }

    private void CalculateArcLayout()
    {
        int count = _items.Count;
        if (count == 0) return;

        var taskbarEdge = DetectTaskbarEdge(out var taskbarRect);
        NativeMethods.GetCursorPos(out var cursor);

        // Origin = the hinge of the fan, right at the taskbar edge
        PointF screenOrigin = taskbarEdge switch
        {
            TaskbarEdge.Top   => new(cursor.X, taskbarRect.Bottom),
            TaskbarEdge.Left  => new(taskbarRect.Right, cursor.Y),
            TaskbarEdge.Right => new(taskbarRect.Left, cursor.Y),
            _                 => new(cursor.X, taskbarRect.Top)
        };

        // Total arc spread, scales gently with item count
        float arcSpread = count > 1
            ? Math.Min(count * ArcSpreadPerItem, MaxArcSpreadDeg)
            : 0f;

        float halfIcon = _iconSize / 2f;
        var relCentres = new PointF[count];

        // Ensure total stack height fits within 75 % of screen
        float totalNeeded = StartDistance + _itemSpacing * (count - 1) + halfIcon;
        if (totalNeeded > _maxStackHeight)
        {
            _itemSpacing = (count > 1)
                ? (_maxStackHeight - StartDistance - halfIcon) / (count - 1)
                : _itemSpacing;
        }

        for (int i = 0; i < count; i++)
        {
            // Interpolation along arc
            float t = count > 1 ? (float)i / (count - 1) : 0f;
            float angleDeg = 90f - (t - 0.5f) * arcSpread; // centred on 90°
            float angleRad = angleDeg * MathF.PI / 180f;

            // Distance grows with each item
            float dist = StartDistance + _itemSpacing * i;

            relCentres[i] = taskbarEdge switch
            {
                TaskbarEdge.Top   => new( dist * MathF.Cos(angleRad),
                                          dist * MathF.Sin(angleRad)),
                TaskbarEdge.Left  => new( dist * MathF.Sin(angleRad),
                                         -dist * MathF.Cos(angleRad)),
                TaskbarEdge.Right => new(-dist * MathF.Sin(angleRad),
                                         -dist * MathF.Cos(angleRad)),
                _                 => new( dist * MathF.Cos(angleRad),
                                         -dist * MathF.Sin(angleRad))
            };
        }

        // Measure text widths for label positioning
        var font = EnsureFont();
        float maxLabelW = 0;
        foreach (var item in _items)
        {
            var sz = TextRenderer.MeasureText(item.Name, font);
            maxLabelW = Math.Max(maxLabelW, sz.Width);
        }
        maxLabelW = Math.Min(maxLabelW, 280); // cap label width

        // ── Bounding box ────
        float extentLeft = maxLabelW + LabelGap; // labels extend to the left of icons
        float minX = float.MaxValue, minY = float.MaxValue;
        float maxX = float.MinValue, maxY = float.MinValue;

        foreach (var c in relCentres)
        {
            minX = Math.Min(minX, c.X - halfIcon - extentLeft);
            minY = Math.Min(minY, c.Y - halfIcon);
            maxX = Math.Max(maxX, c.X + halfIcon);
            maxY = Math.Max(maxY, c.Y + halfIcon);
        }

        minX -= FormMargin;
        minY -= FormMargin;
        maxX += FormMargin;
        maxY += FormMargin;

        int formW = (int)MathF.Ceiling(maxX - minX);
        int formH = (int)MathF.Ceiling(maxY - minY);
        ClientSize = new Size(formW, formH);

        // Screen position
        int formX = (int)(screenOrigin.X + minX);
        int formY = (int)(screenOrigin.Y + minY);

        var screen = Screen.FromPoint(new Point(cursor.X, cursor.Y));
        formX = Math.Max(screen.Bounds.Left, Math.Min(formX, screen.Bounds.Right - formW));
        formY = Math.Max(screen.Bounds.Top, Math.Min(formY, screen.Bounds.Bottom - formH));
        Location = new Point(formX, formY);

        // Icon positions and hit rects in form-local coords
        float offX = -minX;
        float offY = -minY;
        _iconPositions = new PointF[count];
        _hitRects = new RectangleF[count];

        for (int i = 0; i < count; i++)
        {
            float ix = relCentres[i].X + offX - halfIcon;
            float iy = relCentres[i].Y + offY - halfIcon;
            _iconPositions[i] = new PointF(ix, iy);

            // Hit rect covers both icon and label area
            _hitRects[i] = new RectangleF(
                ix - extentLeft,
                iy,
                _iconSize + extentLeft,
                _iconSize);
        }
    }

    private static TaskbarEdge DetectTaskbarEdge(out NativeMethods.RECT taskbarRect)
    {
        var abd = new NativeMethods.APPBARDATA
        {
            cbSize = Marshal.SizeOf<NativeMethods.APPBARDATA>()
        };
        NativeMethods.SHAppBarMessage(NativeMethods.ABM_GETTASKBARPOS, ref abd);
        taskbarRect = abd.rc;

        return abd.uEdge switch
        {
            NativeMethods.ABE_TOP   => TaskbarEdge.Top,
            NativeMethods.ABE_LEFT  => TaskbarEdge.Left,
            NativeMethods.ABE_RIGHT => TaskbarEdge.Right,
            _                       => TaskbarEdge.Bottom
        };
    }

    // ═════════════════════════════════════════════════════════
    //  Painting
    // ═════════════════════════════════════════════════════════

    // WS_EX_LAYERED windows are composited via UpdateLayeredWindow.
    // Suppress WinForms' default GDI painting entirely.
    protected override void OnPaintBackground(PaintEventArgs e) { }
    protected override void OnPaint(PaintEventArgs e) { }

    // ═════════════════════════════════════════════════════════
    //  Layered-window rendering (per-pixel alpha via UpdateLayeredWindow)
    // ═════════════════════════════════════════════════════════

    /// <summary>
    /// Renders the current frame into <see cref="_offscreenBmp"/> and pushes it
    /// to the compositor via <c>UpdateLayeredWindow</c>.  This replaces the old
    /// TransparencyKey + RemoveAlphaFringe approach and gives perfectly smooth,
    /// anti-aliased edges without any alpha quantisation.
    /// </summary>
    private void DrawToLayeredWindow()
    {
        if (!IsHandleCreated || IsDisposed) return;

        int w = ClientSize.Width, h = ClientSize.Height;
        if (w <= 0 || h <= 0) return;

        if (_offscreenBmp == null || _offscreenBmp.Width != w || _offscreenBmp.Height != h)
        {
            _offscreenBmp?.Dispose();
            _offscreenBmp = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        }

        using (var g = Graphics.FromImage(_offscreenBmp))
        {
            g.Clear(Color.Transparent);
            g.SmoothingMode      = SmoothingMode.AntiAlias;
            g.TextRenderingHint  = TextRenderingHint.AntiAliasGridFit;
            g.InterpolationMode  = InterpolationMode.HighQualityBicubic;

            var font = EnsureFont();
            using var textFill   = new SolidBrush(TextNormal);
            float outlineW       = _fontSize / 5f;
            using var outlinePen = new Pen(Color.Black, outlineW) { LineJoin = LineJoin.Round };
            using var haloBrush  = new SolidBrush(Color.FromArgb(180, 0, 0, 0));

            // Insertion sort draw order: lower animProgress drawn first (lower z-order).
            if (_drawOrder.Length != _items.Count) _drawOrder = new int[_items.Count];
            for (int i = 0; i < _drawOrder.Length; i++) _drawOrder[i] = i;
            for (int i = 1; i < _drawOrder.Length; i++)
            {
                int   key  = _drawOrder[i];
                float keyP = key < _animProgress.Length ? _animProgress[key] : 0f;
                int   j    = i - 1;
                while (j >= 0 && (_drawOrder[j] < _animProgress.Length
                    ? _animProgress[_drawOrder[j]] : 0f) > keyP)
                { _drawOrder[j + 1] = _drawOrder[j]; j--; }
                _drawOrder[j + 1] = key;
            }

            foreach (int i in _drawOrder)
                DrawItem(g, i, font, textFill, outlinePen, haloBrush, _sf!);
        }

        // UpdateLayeredWindow requires premultiplied alpha.
        // Premultiply in-place (cleared on next call via g.Clear).
        PremultiplyBitmap(_offscreenBmp);

        IntPtr hdcScreen = NativeMethods.GetDC(IntPtr.Zero);
        IntPtr hdcMem    = NativeMethods.CreateCompatibleDC(hdcScreen);
        IntPtr hBmp      = _offscreenBmp.GetHbitmap(Color.FromArgb(0));
        IntPtr oldBmp    = NativeMethods.SelectObject(hdcMem, hBmp);

        var size  = new NativeMethods.SIZE  { cx = w, cy = h };
        var ptSrc = new NativeMethods.POINT { X = 0, Y = 0 };
        var blend = new NativeMethods.BLENDFUNCTION
        {
            BlendOp             = 0,             // AC_SRC_OVER
            BlendFlags          = 0,
            SourceConstantAlpha = _currentAlpha, // drives fade-in
            AlphaFormat         = 1              // AC_SRC_ALPHA
        };

        NativeMethods.UpdateLayeredWindow(
            Handle, hdcScreen, IntPtr.Zero,
            ref size, hdcMem, ref ptSrc,
            0, ref blend, NativeMethods.ULW_ALPHA);

        NativeMethods.SelectObject(hdcMem, oldBmp);
        NativeMethods.DeleteObject(hBmp);
        NativeMethods.DeleteDC(hdcMem);
        NativeMethods.ReleaseDC(IntPtr.Zero, hdcScreen);
    }

    /// <summary>
    /// Premultiplies the alpha channel of <paramref name="bmp"/> in-place.
    /// Required by <c>UpdateLayeredWindow</c> which expects premultiplied ARGB.
    /// </summary>
    private static unsafe void PremultiplyBitmap(Bitmap bmp)
    {
        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect,
            System.Drawing.Imaging.ImageLockMode.ReadWrite,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        byte* p = (byte*)data.Scan0;
        int n = bmp.Width * bmp.Height;
        for (int i = 0; i < n; i++)
        {
            int  off = i * 4;
            byte a   = p[off + 3];
            if (a == 0) { p[off] = p[off + 1] = p[off + 2] = 0; }
            else if (a < 255)
            {
                p[off]     = (byte)(p[off]     * a / 255);
                p[off + 1] = (byte)(p[off + 1] * a / 255);
                p[off + 2] = (byte)(p[off + 2] * a / 255);
            }
        }
        bmp.UnlockBits(data);
    }

    private void DrawItem(Graphics g, int i, Font font, Brush textFill,
        Pen outlinePen, Brush haloBrush, StringFormat sf)
    {
        var item = _items[i];
        var iconPos = _iconPositions[i];

        // ── Animated scale via eased progress ──
        float t = (i < _animProgress.Length) ? EaseInOut(_animProgress[i]) : 0f;
        float scale = 1f + t * (HoverScaleMax - 1f);
        float drawSize = _iconSize * scale;
        float drawX = iconPos.X - (_iconSize * (scale - 1f) / 2f);
        float drawY = iconPos.Y - (_iconSize * (scale - 1f) / 2f);

        // ── Icon or Arrow ──
        if (item.IsArrow)
        {
            DrawArrow(g, drawX, drawY, drawSize);
        }
        else if (item.Bmp != null)
        {
            g.DrawImage(item.Bmp, new RectangleF(drawX, drawY, drawSize, drawSize));
        }

        // ── Text label to the left of the icon (outlined) ──
        float labelRight = iconPos.X - LabelGap;
        float labelLeft = labelRight - 270;
        if (labelLeft < 0) labelLeft = 0;
        float labelW = labelRight - labelLeft;

        if (labelW > 0)
        {
            // Interpolate font size: slightly larger when hovered
            float textScale = 1f + t * 0.08f; // up to 8 % bigger
            float emSize = g.DpiY * _fontSize * textScale / 72f; // pt → px
            float textY = iconPos.Y + (_iconSize - emSize) / 2f;

            var layoutRect = new RectangleF(labelLeft, textY, labelW, emSize + 4);
            int fontStyle = (int)FontStyle.Bold;

            // Dark halo: draw text shape offset in 8 directions for a soft glow.
            // Reuse a single GraphicsPath (Reset() clears figures without allocating).
            float haloOff = outlinePen.Width * 0.6f;
            float[] offsets = [-haloOff, 0, haloOff];
            using var haloPath = new GraphicsPath();
            foreach (float dx in offsets)
            {
                foreach (float dy in offsets)
                {
                    if (dx == 0 && dy == 0) continue;
                    haloPath.Reset();
                    var haloRect = layoutRect;
                    haloRect.Offset(dx, dy);
                    haloPath.AddString(item.Name, font.FontFamily, fontStyle,
                        emSize, haloRect, sf);
                    g.FillPath(haloBrush, haloPath);
                }
            }

            // Crisp outlined text on top
            using var textPath = new GraphicsPath();
            textPath.AddString(item.Name, font.FontFamily, fontStyle,
                emSize, layoutRect, sf);

            g.DrawPath(outlinePen, textPath);   // black outline
            g.FillPath(textFill, textPath);     // white fill
        }
    }

    private Font CreateItemFont()
    {
        try { return new Font("Segoe UI Variable Text", _fontSize, FontStyle.Bold, GraphicsUnit.Point); }
        catch { return new Font("Segoe UI", _fontSize, FontStyle.Bold, GraphicsUnit.Point); }
    }

    private Font EnsureFont() => _font ??= CreateItemFont();

    /// <summary>
    /// Draws a macOS-style "open folder" button: a squircle with a dark blue-grey
    /// gradient and a white upward-pointing chevron with rounded caps.
    /// Corner radius is kept ≤ boxSz/2 to prevent arc overlap in GraphicsPath.
    /// </summary>
    private static void DrawArrow(Graphics g, float x, float y, float size)
    {
        float cx = x + size / 2f;
        float cy = y + size / 2f;
        float pad   = size * 0.08f;
        float boxSz = size - pad * 2f;
        // Corner radius must satisfy cr * 2 < boxSz to avoid arc overlap.
        // 0.22 gives iOS-style proportions (~22 % of side length).
        float cr = boxSz * 0.22f;

        var rect = new RectangleF(cx - boxSz / 2f, cy - boxSz / 2f, boxSz, boxSz);

        // ── Squircle background ───────────────────────────────────────────
        using var bgPath = SquirclePath(rect, cr);

        using var bgGrad = new System.Drawing.Drawing2D.LinearGradientBrush(
            new PointF(rect.Left, rect.Top),
            new PointF(rect.Left, rect.Bottom),
            Color.FromArgb(255, 78,  88, 112),
            Color.FromArgb(255, 36,  42,  62));
        g.FillPath(bgGrad, bgPath);

        // Thin dark border for definition
        using var borderPen = new Pen(Color.FromArgb(255, 22, 26, 44), size * 0.018f);
        g.DrawPath(borderPen, bgPath);

        // ── White chevron: ^ drawn with two rounded strokes ───────────────
        float chevW = boxSz * 0.27f;
        float chevH = boxSz * 0.17f;
        float chevCY = cy + chevH * 0.18f;  // slightly below centre

        using var chevPen = new Pen(Color.White, size * 0.095f)
        {
            StartCap = System.Drawing.Drawing2D.LineCap.Round,
            EndCap   = System.Drawing.Drawing2D.LineCap.Round,
            LineJoin = System.Drawing.Drawing2D.LineJoin.Round
        };
        g.DrawLine(chevPen, cx - chevW, chevCY + chevH, cx, chevCY);
        g.DrawLine(chevPen, cx,         chevCY,         cx + chevW, chevCY + chevH);
    }

    /// <summary>
    /// Builds a rounded-rectangle (squircle) GraphicsPath.
    /// Requires cr * 2 &lt; rect.Width and cr * 2 &lt; rect.Height.
    /// </summary>
    private static GraphicsPath SquirclePath(RectangleF r, float cr)
    {
        float d = cr * 2f;
        var p = new GraphicsPath();
        p.AddArc(r.X,             r.Y,              d, d, 180, 90);
        p.AddArc(r.Right - d,     r.Y,              d, d, 270, 90);
        p.AddArc(r.Right - d,     r.Bottom - d,     d, d,   0, 90);
        p.AddArc(r.X,             r.Bottom - d,     d, d,  90, 90);
        p.CloseFigure();
        return p;
    }

    /// <summary>
    /// Writes <paramref name="src"/> into <paramref name="dst"/>, converting each pixel
    /// to either fully transparent or fully opaque (required for TransparencyKey rendering).
    /// <para>
    /// The offscreen bitmap is rendered on a transparent (0,0,0,0) background, so
    // ═════════════════════════════════════════════════════════
    //  Mouse Interaction
    // ═════════════════════════════════════════════════════════

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button == MouseButtons.Left)
        {
            _mouseDownPos = e.Location;
            _mouseDownIndex = HitTest(e.Location);
            _dragging = false;
        }
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);

        // Start drag if mouse moved beyond system drag threshold
        if (!_dragging && e.Button == MouseButtons.Left && _mouseDownIndex >= 0
            && _mouseDownIndex < _items.Count)
        {
            int dx = e.X - _mouseDownPos.X;
            int dy = e.Y - _mouseDownPos.Y;
            if (Math.Abs(dx) > SystemInformation.DragSize.Width / 2
                || Math.Abs(dy) > SystemInformation.DragSize.Height / 2)
            {
                _dragging = true;
                var item = _items[_mouseDownIndex];
                if (!string.IsNullOrEmpty(item.FullPath))
                {
                    ShellDragHelper.DoDragDrop(this, item.FullPath,
                        DragDropEffects.Move | DragDropEffects.Copy,
                        item.Icon);
                    // After drop completes, close the fan
                    Close();
                    return;
                }
            }
        }

        int idx = HitTest(e.Location);
        if (idx != _hoveredIndex)
        {
            // Snap hover-out instantly for previous item
            if (_hoveredIndex >= 0 && _hoveredIndex < _animProgress.Length)
                _animProgress[_hoveredIndex] = 0f;
            _hoveredIndex = idx;
            _animTimer?.Start(); // ensure running for hover animation
            Cursor = idx >= 0 ? Cursors.Hand : Cursors.Default;
            DrawToLayeredWindow();
        }
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        _mouseDownIndex = -1;
        _dragging = false;
    }

    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);

        if (_hoveredIndex != -1)
        {
            // Snap hover-out instantly
            if (_hoveredIndex < _animProgress.Length)
                _animProgress[_hoveredIndex] = 0f;
            _hoveredIndex = -1;
            Cursor = Cursors.Default;
            DrawToLayeredWindow();
        }
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        e.Handled = true;
        Close();
    }

    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);

        // Don't launch if we just finished a drag
        if (_dragging) return;
        if (e.Button != MouseButtons.Left) return;

        int idx = HitTest(e.Location);
        if (idx < 0 || idx >= _items.Count) return;

        var item = _items[idx];
        if (string.IsNullOrEmpty(item.FullPath)) return;

        LaunchItem(item);
        Close();
    }

    private int HitTest(Point pt)
    {
        // Check in reverse so topmost (visually) items take priority
        for (int i = _hitRects.Length - 1; i >= 0; i--)
        {
            if (_hitRects[i].Contains(pt))
                return i;
        }
        return -1;
    }

    private static void LaunchItem(FanItem item)
    {
        try
        {
            if (item.IsDirectory)
            {
                Process.Start(new ProcessStartInfo("explorer.exe", $"\"{item.FullPath}\"")
                {
                    UseShellExecute = false
                });
            }
            else
            {
                Process.Start(new ProcessStartInfo(item.FullPath)
                {
                    UseShellExecute = true
                });
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Could not open:\n{item.FullPath}\n\n{ex.Message}",
                "Fan Folder",
                MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
        }
    }

    // ═════════════════════════════════════════════════════════
    //  Window Style
    // ═════════════════════════════════════════════════════════

    protected override bool ShowWithoutActivation => false;

    /// <summary>
    /// Make every pixel of the layered window hittable — including fully
    /// transparent regions.  Without this, WS_EX_LAYERED windows only fire
    /// mouse events over non-zero-alpha pixels, so hovering over the
    /// transparent label background would fall through to the window below.
    /// Returning HTCLIENT unconditionally routes all pointer messages to us;
    /// the existing HitTest() logic still decides which item (if any) was hit.
    /// </summary>
    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST = 0x0084;
        const int HTCLIENT     = 1;
        if (m.Msg == WM_NCHITTEST)
        {
            m.Result = (IntPtr)HTCLIENT;
            return;
        }
        base.WndProc(ref m);
    }

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= 0x00000080; // WS_EX_TOOLWINDOW – hide from Alt+Tab
            cp.ExStyle |= 0x00080000; // WS_EX_LAYERED    – per-pixel alpha via UpdateLayeredWindow
            return cp;
        }
    }

    // ═════════════════════════════════════════════════════════
    //  Cleanup
    // ═════════════════════════════════════════════════════════

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _animTimer?.Stop();
            _animTimer?.Dispose();
            _fadeTimer?.Stop();
            _fadeTimer?.Dispose();
            _font?.Dispose();
            _sf?.Dispose();
            _offscreenBmp?.Dispose();
            foreach (var item in _items)
            {
                item.Bmp?.Dispose();
                item.Icon?.Dispose();
            }
        }
        base.Dispose(disposing);
    }
}
