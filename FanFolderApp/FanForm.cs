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
    private static readonly Color HoverTint = Color.FromArgb(60, 255, 255, 255);
    private static readonly Color TranspKey = Color.Magenta;

    // ─── Animation ──────────────────────────────────────────────
    private const float HoverScaleMax = 1.4f;
    private const float AnimSpeed = 0.45f;  // progress per tick (≈ 2-3 ticks to full)
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

    // ─── Inner Model ─────────────────────────────────────────
    private sealed class FanItem
    {
        public required string Name { get; init; }
        public required string FullPath { get; init; }
        public required bool IsDirectory { get; init; }
        public Icon? Icon { get; set; }
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
        DoubleBuffered = true;

        TransparencyKey = TranspKey;
        BackColor = TranspKey;
        KeyPreview = true;
        Opacity = 0; // start invisible for fade-in

        ComputeAdaptiveSizes();
        LoadItems();
        CalculateArcLayout();
        InitAnimation();
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        // Capture final position and offset downward for slide-up
        _targetTop = Top;
        Top += SlideDistance;
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

        // Ease-out: fast start, smooth deceleration
        float t = 1f - MathF.Pow(1f - _fadeProgress, 3f);

        Opacity = t;
        Top = _targetTop + (int)(SlideDistance * (1f - t));
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

        for (int i = 0; i < _animProgress.Length; i++)
        {
            float target = (i == _hoveredIndex) ? 1f : 0f;
            float cur = _animProgress[i];

            if (MathF.Abs(cur - target) < 0.005f)
            {
                if (cur != target) { _animProgress[i] = target; needsRepaint = true; }
                continue;
            }

            // Move linearly; easing is applied when reading the value
            _animProgress[i] += (target > cur) ? AnimSpeed : -AnimSpeed;
            _animProgress[i] = Math.Clamp(_animProgress[i], 0f, 1f);
            needsRepaint = true;
        }

        if (needsRepaint) Invalidate();
    }

    /// <summary>Cubic ease-in-out: smooth acceleration and deceleration.</summary>
    private static float EaseInOut(float t)
    {
        return t < 0.5f
            ? 4f * t * t * t
            : 1f - MathF.Pow(-2f * t + 2f, 3f) / 2f;
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
                Icon = FileService.GetShellIcon(entry.FullName)
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
        using var font = CreateItemFont();
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

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        // Render everything onto a transparent offscreen bitmap so that
        // anti-aliased edges never blend with the magenta TransparencyKey.
        using var offscreen = new Bitmap(
            ClientSize.Width, ClientSize.Height,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);

        using (var g = Graphics.FromImage(offscreen))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;

            using var font = CreateItemFont();
            using var textFill = new SolidBrush(TextNormal);
            float outlineW = _fontSize / 5f;
            using var outlinePen = new Pen(Color.Black, outlineW) { LineJoin = LineJoin.Round };
            using var haloBrush = new SolidBrush(Color.FromArgb(180, 0, 0, 0));

            var sf = new StringFormat
            {
                Alignment = StringAlignment.Far,
                LineAlignment = StringAlignment.Center,
                Trimming = StringTrimming.EllipsisCharacter,
                FormatFlags = StringFormatFlags.NoWrap
            };

            // Build sorted draw order: items with higher animProgress drawn later (on top)
            var drawOrder = Enumerable.Range(0, _items.Count)
                .OrderBy(i => _animProgress.Length > i ? _animProgress[i] : 0f)
                .ToList();

            foreach (int i in drawOrder)
            {
                DrawItem(g, i, font, textFill, outlinePen, haloBrush, sf);
            }
        }

        // Threshold alpha on the entire frame to eliminate semi-transparent
        // pixels that would blend with magenta and produce a pink fringe.
        using var clean = RemoveAlphaFringe(offscreen);
        e.Graphics.DrawImageUnscaled(clean, 0, 0);
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

        // ── Icon ──
        if (item.Icon != null)
        {
            using var bmp = item.Icon.ToBitmap();
            g.DrawImage(bmp, new RectangleF(drawX, drawY, drawSize, drawSize));
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

            // Dark halo: draw the text shape offset in multiple
            // directions to create a soft dark background glow
            float haloOff = outlinePen.Width * 0.6f;
            float[] offsets = [-haloOff, 0, haloOff];
            foreach (float dx in offsets)
            {
                foreach (float dy in offsets)
                {
                    if (dx == 0 && dy == 0) continue;
                    using var haloPath = new GraphicsPath();
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

    /// <summary>
    /// Thresholds alpha: pixels with alpha &lt; 128 become fully transparent,
    /// the rest become fully opaque.  This prevents semi-transparent edges
    /// from blending with the magenta TransparencyKey and creating a pink halo.
    /// </summary>
    private static unsafe Bitmap RemoveAlphaFringe(Bitmap src)
    {
        var dst = new Bitmap(src.Width, src.Height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        var rect = new Rectangle(0, 0, src.Width, src.Height);

        var srcData = src.LockBits(rect, System.Drawing.Imaging.ImageLockMode.ReadOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        var dstData = dst.LockBits(rect, System.Drawing.Imaging.ImageLockMode.WriteOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);

        int pixelCount = src.Width * src.Height;
        byte* pSrc = (byte*)srcData.Scan0;
        byte* pDst = (byte*)dstData.Scan0;

        for (int i = 0; i < pixelCount; i++)
        {
            int off = i * 4;
            byte a = pSrc[off + 3];

            if (a < 128)
            {
                // Fully transparent
                pDst[off]     = 0;
                pDst[off + 1] = 0;
                pDst[off + 2] = 0;
                pDst[off + 3] = 0;
            }
            else
            {
                // Fully opaque
                pDst[off]     = pSrc[off];     // B
                pDst[off + 1] = pSrc[off + 1]; // G
                pDst[off + 2] = pSrc[off + 2]; // R
                pDst[off + 3] = 255;
            }
        }

        src.UnlockBits(srcData);
        dst.UnlockBits(dstData);
        return dst;
    }

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
                        DragDropEffects.Move | DragDropEffects.Copy);
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
            // Snap hover-in instantly so it feels snappy
            if (idx >= 0 && idx < _animProgress.Length)
                _animProgress[idx] = 1f;
            Cursor = idx >= 0 ? Cursors.Hand : Cursors.Default;
            Invalidate();
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
            Invalidate();
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

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= 0x00000080; // WS_EX_TOOLWINDOW – hide from Alt+Tab
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
            foreach (var item in _items)
                item.Icon?.Dispose();
        }
        base.Dispose(disposing);
    }
}
