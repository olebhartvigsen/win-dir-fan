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
    private const float HoverScaleMax  = 1.4f;
    private const float HoverSpeedIn   = 0.30f;  // 0→1 in ~3 ticks (~50 ms)
    private const float HoverSpeedOut  = 0.38f;  // 1→0 in ~3 ticks (~42 ms)
    private static readonly Color TextHover = Color.FromArgb(255, 255, 230, 120); // warm gold

    // ─── Entry Animation ────────────────────────────────────────
    private System.Diagnostics.Stopwatch _entrySw = new();
    private float   _entryElapsed;          // ms since OnShown (real time)
    private bool    _entryDone;
    private float[] _entryProgress = [];    // per-item 0→1
    private PointF  _arcOriginInForm;       // arc hinge in form-local coords
    private readonly AnimStyle _animStyle;

    // ─── State ───────────────────────────────────────────────────
    private readonly List<FanItem> _items = [];
    private readonly string   _folderPath;
    private readonly SortMode _sortMode;
    private readonly int      _maxItems;
    private readonly bool     _includeDirs;
    private readonly string?  _filterRegex;
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
    private float _layoutMinX;            // saved arc origin→form-edge offset for Reposition()
    private float _layoutMinY;
    private static int _lastTaskbarAnchorX = -1; // updated on real taskbar clicks; reused for Alt+Tab

    // ─── Rendering Cache ─────────────────────────────────────────────
    private Bitmap? _offscreenBmp;  // 1× final bitmap fed to UpdateLayeredWindow
    private Bitmap? _hiresBmp;      // 2× super-sampled render buffer (SSAA)
    private byte    _currentAlpha;   // 0–255; drives UpdateLayeredWindow fade
    private Font?   _font;
    private StringFormat? _sf;
    private int[] _drawOrder = [];
    private CancellationTokenSource _iconLoadCts = new();
    private bool _dirty;   // render needed on next timer tick
    private bool _contextMenuOpen; // true while the right-click context menu is visible
    private NativeMethods.IContextMenu2? _activeContextMenu2; // forwarded WM_INITMENUPOPUP etc.

    // ─── Inner Model ─────────────────────────────────────────
    private sealed class FanItem
    {
        public required string Name { get; init; }
        public required string FullPath { get; init; }
        public required bool IsDirectory { get; init; }
        public bool IsArrow { get; init; }
        public Icon? Icon { get; set; }
        public Bitmap? Bmp { get; set; }
        public float MeasuredTextWidth { get; set; } = -1f; // cached pill width (-1 = uncached)
    }

    /// <summary>True while the right-click context menu is open — used by the global
    /// mouse hook to suppress fan close on clicks that land on the menu.</summary>
    internal bool IsContextMenuOpen => _contextMenuOpen;

    /// <summary>Raised after a shell context-menu command modifies the file system
    /// (e.g. Delete, Rename).  MainHiddenForm subscribes to this to restart the
    /// prewarm scan after the operation completes.</summary>
    internal event EventHandler? FileSystemModified;

    // ═════════════════════════════════════════════════════════
    //  Construction
    // ═════════════════════════════════════════════════════════

    public FanForm(string   folderPath,
                   SortMode sortMode    = SortMode.DateModifiedDesc,
                   int      maxItems    = 15,
                   bool     includeDirs = true,
                   string?  filterRegex = null,
                   IReadOnlyList<FileSystemInfo>? preloadedItems = null,
                   AnimStyle animStyle  = AnimStyle.Fan)
    {
        _folderPath  = folderPath;
        _sortMode    = sortMode;
        _maxItems    = maxItems;
        _includeDirs = includeDirs;
        _filterRegex = filterRegex;
        _animStyle   = animStyle;

        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        KeyPreview = true;

        ComputeAdaptiveSizes();
        LoadItems(preloadedItems);
        CalculateArcLayout();
        _ = LoadIconsAsync();
        InitAnimation();
        _sf = new StringFormat
        {
            Alignment     = StringAlignment.Far,
            LineAlignment = StringAlignment.Center,
            Trimming      = StringTrimming.None,      // never ellipsize — pill is sized to fit
            FormatFlags   = StringFormatFlags.NoWrap | StringFormatFlags.NoClip
        };
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        ApplyInputRegion();

        _entryElapsed  = 0f;
        _entryDone     = _animStyle == AnimStyle.None;
        _entryProgress = new float[_items.Count];
        _entrySw.Restart();

        // Fan/Glide: full alpha immediately — items fade in via per-item alpha
        // Spring: start transparent so the group fade-in is handled at window level
        _currentAlpha = _animStyle == AnimStyle.Spring ? (byte)0 : (byte)255;

        // Single shared timer drives both entry animation and hover animation — no double-draw.
        _animTimer?.Start();
        DrawToLayeredWindow();
    }

    // ─── Fan Entry ──────────────────────────────────────────────
    // Items radiate one-by-one from the arc hinge to their final positions.
    private const float FanStaggerMs   = 30f;
    private const float FanItemMs      = 330f;

    private void UpdateFanEntry()
    {
        bool allDone = true;
        for (int i = 0; i < _entryProgress.Length; i++)
        {
            float start = i * FanStaggerMs;
            float t     = Math.Clamp((_entryElapsed - start) / FanItemMs, 0f, 1f);
            _entryProgress[i] = EaseOutQuart(t);
            if (t < 1f) allDone = false;
        }
        _entryDone = allDone;
    }

    // ─── Glide Entry ────────────────────────────────────────────
    // All items drift up 30px and fade in together (per-item alpha, not window alpha).
    private const float GlideDurationMs = 800f;
    private const float GlideShiftPx    = 32f;

    private void UpdateGlideEntry()
    {
        float t = EaseOutCubic(Math.Clamp(_entryElapsed / GlideDurationMs, 0f, 1f));
        for (int i = 0; i < _entryProgress.Length; i++) _entryProgress[i] = t;
        _currentAlpha = 255; // window always opaque; per-item alpha drives the fade
        _entryDone    = _entryElapsed >= GlideDurationMs;
    }

    // ─── Spring Entry ───────────────────────────────────────────
    // Window fades in fast, then items spring-scale 0→1 with stagger.
    // 15 items: 14 × 28ms stagger + 420ms item = 812ms total
    private const float SpringFadeMs    = 120f;
    private const float SpringStaggerMs = 28f;
    private const float SpringItemMs    = 420f;

    private void UpdateSpringEntry()
    {
        // Group alpha
        float alphaT   = Math.Clamp(_entryElapsed / SpringFadeMs, 0f, 1f);
        _currentAlpha  = (byte)Math.Clamp((int)(EaseOutQuart(alphaT) * 255f), 0, 255);

        bool allDone = true;
        for (int i = 0; i < _entryProgress.Length; i++)
        {
            float start = i * SpringStaggerMs;
            float t     = Math.Clamp((_entryElapsed - start) / SpringItemMs, 0f, 1f);
            _entryProgress[i] = SpringOut(t);
            if (t < 1f) allDone = false;
        }
        _entryDone = allDone;
        if (_entryDone) _currentAlpha = 255;
    }

    private void InitAnimation()
    {
        _animProgress = new float[_items.Count];
        _animTimer = new System.Windows.Forms.Timer { Interval = 16 }; // ~60 fps
        _animTimer.Tick += OnAnimTick;
        // Timer is started in OnShown so entry + hover share one tick
    }

    private void OnAnimTick(object? sender, EventArgs e)
    {
        // ── Entry animation (runs until _entryDone) ──
        bool needsRepaint = false;
        if (!_entryDone && _entryProgress.Length > 0)
        {
            _entryElapsed = (float)_entrySw.Elapsed.TotalMilliseconds;

            switch (_animStyle)
            {
                case AnimStyle.Fan:    UpdateFanEntry();    break;
                case AnimStyle.Glide:  UpdateGlideEntry();  break;
                case AnimStyle.Spring: UpdateSpringEntry(); break;
                default: _entryDone = true; _currentAlpha = 255; break;
            }
            needsRepaint = true;
        }

        // ── Hover animation ──
        bool allSettled = true;
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
            float speed = target > cur ? HoverSpeedIn : HoverSpeedOut;
            _animProgress[i] += target > cur ? speed : -speed;
            _animProgress[i] = Math.Clamp(_animProgress[i], 0f, 1f);
            needsRepaint = true;
        }

        if (needsRepaint || _dirty) { _dirty = false; DrawToLayeredWindow(); }

        // Stop when both entry and hover animations are fully settled.
        if (_entryDone && allSettled && !_dirty) _animTimer?.Stop();
    }

    /// <summary>Ease-out cubic: fast start, smooth deceleration — no initial delay.</summary>
    private static float EaseInOut(float t)          => 1f - MathF.Pow(1f - t, 3f);
    private static float EaseOutQuart(float t)       => 1f - MathF.Pow(1f - t, 4f);
    private static float EaseOutExpo(float t)        => t >= 1f ? 1f : 1f - MathF.Pow(2f, -10f * t);
    private static float EaseOutCubic(float t)       => 1f - MathF.Pow(1f - t, 3f);
    private static float SpringOut(float t)
    {
        // Smooth spring with ~7% overshoot around t≈0.65
        return EaseOutCubic(t) + MathF.Sin(t * MathF.PI) * 0.12f;
    }

    // Returns (dx, dy offset from rest, scale multiplier, alpha 0-255) for item i during entry.
    private (float dx, float dy, float scale, float alpha) GetEntryAnim(int i)
    {
        if (_entryDone || _animStyle == AnimStyle.None)
            return (0f, 0f, 1f, 255f);

        float p = i < _entryProgress.Length ? _entryProgress[i] : 0f;

        switch (_animStyle)
        {
            case AnimStyle.Fan:
            {
                // Fly from arc hinge to rest position
                var rest = _iconPositions[i];
                float fromX = _arcOriginInForm.X - _iconSize * 0.5f;
                float fromY = _arcOriginInForm.Y - _iconSize * 0.5f;
                float dx    = (fromX - rest.X) * (1f - p);
                float dy    = (fromY - rest.Y) * (1f - p);
                return (dx, dy, p, p * 255f);
            }
            case AnimStyle.Glide:
            {
                // All items slide up and fade in simultaneously
                float alpha = Math.Clamp(p * 255f, 0f, 255f);
                return (0f, GlideShiftPx * (1f - p), 1f, alpha);
            }
            case AnimStyle.Spring:
            {
                // Scale and alpha grow together — items fade in as they grow,
                // preventing any abrupt pop at small sizes.
                float scale = Math.Max(p, 0f);
                float alpha = Math.Clamp(p * 255f, 0f, 255f);
                return (0f, 0f, scale, alpha);
            }
            default:
                return (0f, 0f, 1f, 255f);
        }
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

    private void LoadItems(IReadOnlyList<FileSystemInfo>? preloadedItems)
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

        var entries = preloadedItems ?? FileService.GetRecentItems(_folderPath, _sortMode, _maxItems, _includeDirs, _filterRegex);

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
        int targetSize = (int)(_iconSize * 2); // 2× for SSAA buffer
        var tasks = _items
            .Where(item => !item.IsArrow && !string.IsNullOrEmpty(item.FullPath))
            .Select(item =>
            {
                var path = item.FullPath;
                return Task.Run(() =>
                {
                    // For image files, show a thumbnail of the actual content.
                    var thumbBmp = FileService.GetImageThumbnail(path, targetSize);
                    if (thumbBmp != null)
                        return (item, icon: (Icon?)null, bmp: (Bitmap?)thumbBmp);

                    var icon = FileService.GetShellIcon(path);
                    var bmp  = FileService.GetShellBitmap(path, targetSize);
                    return (item, icon, bmp);
                });
            })
            .ToList();

        foreach (var loadTask in tasks)
        {
            var (item, icon, bmp) = await loadTask;
            if (IsDisposed || _iconLoadCts.IsCancellationRequested)
            {
                icon?.Dispose();
                bmp?.Dispose();
                return;
            }
            item.Icon = icon;
            item.Bmp  = bmp;
            _dirty = true;
            _animTimer?.Start();
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

        // Always use the spacing that 15 items would produce when filling the
        // stack height — this keeps density constant regardless of item count,
        // so the menu gets proportionally shorter with fewer items.
        const int BaselineItems = 15;
        float baselineSpacing = (BaselineItems > 1)
            ? (_maxStackHeight - StartDistance - halfIcon) / (BaselineItems - 1)
            : _itemSpacing;
        _itemSpacing = baselineSpacing;

        // If the actual item count still overflows (e.g. MaxItems > 15), compress further.
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

        // Measure all items at max hover scale with GDI+ (same engine as DrawString)
        // to ensure the form is wide enough that no label clips during hover.
        float maxLabelW;
        {
            const float MaxHoverScale = 1.08f;
            const float PillPadH      = 8f;
            using var hoverFont = new Font(EnsureFont().FontFamily,
                _fontSize * MaxHoverScale, FontStyle.Bold, GraphicsUnit.Point);
            using var measSf = new StringFormat
            {
                Alignment   = StringAlignment.Far,
                FormatFlags = StringFormatFlags.NoWrap | StringFormatFlags.NoClip
            };
            using var tmpBmp = new Bitmap(1, 1);
            using var tmpG   = Graphics.FromImage(tmpBmp);
            float widest = _items.Count == 0 ? 200f
                : _items.Max(it => tmpG.MeasureString(it.Name, hoverFont,
                    new SizeF(float.MaxValue, 32f), measSf).Width);
            // pill padding both sides + 6 px sub-pixel safety buffer
            maxLabelW = MathF.Min(widest + PillPadH * 2f + 6f, 600f);
        }

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
        _layoutMinX = minX;
        _layoutMinY = minY;
        int formX = (int)(screenOrigin.X + minX);
        int formY = (int)(screenOrigin.Y + minY);

        var screen = Screen.FromPoint(new Point(cursor.X, cursor.Y));
        formX = Math.Max(screen.Bounds.Left, Math.Min(formX, screen.Bounds.Right - formW));
        formY = Math.Max(screen.Bounds.Top, Math.Min(formY, screen.Bounds.Bottom - formH));
        Location = new Point(formX, formY);

        // Icon positions and hit rects in form-local coords
        float offX = -minX;
        float offY = -minY;
        _arcOriginInForm = new PointF(offX, offY); // arc hinge maps to form origin
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

    /// <summary>
    /// Restricts the window's interactive surface to the union of all icon+label
    /// hit rectangles (plus a small padding for the hover-scale effect).
    /// Mouse events over the transparent areas of the form's bounding rectangle
    /// are never routed to this window — eliminating the WM_NCHITTEST overhead
    /// that caused cursor jitter when the fan was open.
    /// </summary>
    private void ApplyInputRegion()
    {
        if (!IsHandleCreated || _hitRects.Length == 0) return;

        int pad = (int)(_iconSize * 0.25f); // headroom for HoverScaleMax expansion
        using var rgn = new System.Drawing.Region(RectangleF.Empty);
        foreach (var r in _hitRects)
            rgn.Union(new RectangleF(r.X - pad, r.Y - pad,
                                     r.Width + pad * 2, r.Height + pad * 2));

        using var g = CreateGraphics();
        IntPtr hRgn = rgn.GetHrgn(g);
        // SetWindowRgn takes ownership of hRgn — do not DeleteObject it.
        NativeMethods.SetWindowRgn(Handle, hRgn, false);
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

    /// <summary>
    /// Re-snaps the form to the current cursor position using the saved layout
    /// offsets — no TextRenderer, no arc math, just a cursor read + SetWindowPos.
    /// Call this on a pre-warmed form just before Show().
    /// </summary>
    public void Reposition()
    {
        var edge = DetectTaskbarEdge(out var taskbarRect);
        NativeMethods.GetCursorPos(out var cursor);

        // Try to locate our taskbar button via the win32 window hierarchy.
        // When found, the button rect is authoritative for both real clicks and
        // Alt+Tab (the cursor may be anywhere on the taskbar in either case).
        int btnCenter = FindTaskbarButtonCenter(taskbarRect, out var btnRect);
        bool buttonFound = btnRect.Right > btnRect.Left;

        // Cursor is on the taskbar → could be a real click or Alt+Tab.
        bool cursorOnTaskbar = cursor.X >= taskbarRect.Left && cursor.X <= taskbarRect.Right
                            && cursor.Y >= taskbarRect.Top  && cursor.Y <= taskbarRect.Bottom;

        int anchorX;
        int anchorY;

        if (buttonFound)
        {
            // Win10: we have the exact button rect — always use its center.
            anchorX = (edge == TaskbarEdge.Bottom || edge == TaskbarEdge.Top)
                ? btnCenter : cursor.X;
            anchorY = (edge == TaskbarEdge.Bottom || edge == TaskbarEdge.Top)
                ? cursor.Y : btnCenter;
            _lastTaskbarAnchorX = btnCenter;
        }
        else if (cursorOnTaskbar)
        {
            // Win11 / button not found: cursor on taskbar → treat as real click,
            // use cursor position and cache it for future Alt+Tab.
            anchorX = cursor.X;
            anchorY = cursor.Y;
            _lastTaskbarAnchorX = (edge == TaskbarEdge.Bottom || edge == TaskbarEdge.Top)
                ? cursor.X : cursor.Y;
        }
        else
        {
            // Alt+Tab, cursor off taskbar: use cached position or taskbar center.
            anchorX = cursor.X;
            anchorY = cursor.Y;
            if (edge == TaskbarEdge.Bottom || edge == TaskbarEdge.Top)
                anchorX = btnCenter; // btnCenter falls back to cache or taskbar center
            else
                anchorY = btnCenter;
        }

        PointF origin = edge switch
        {
            TaskbarEdge.Top   => new(anchorX, taskbarRect.Bottom),
            TaskbarEdge.Left  => new(taskbarRect.Right, anchorY),
            TaskbarEdge.Right => new(taskbarRect.Left,  anchorY),
            _                 => new(anchorX, taskbarRect.Top)
        };

        int formX = (int)(origin.X + _layoutMinX);
        int formY = (int)(origin.Y + _layoutMinY);
        var screen = Screen.FromPoint(new Point(anchorX, anchorY));
        formX = Math.Max(screen.Bounds.Left, Math.Min(formX, screen.Bounds.Right  - Width));
        formY = Math.Max(screen.Bounds.Top,  Math.Min(formY, screen.Bounds.Bottom - Height));
        Location = new Point(formX, formY);
    }

    /// <summary>
    /// Finds the horizontal center of this app's taskbar button by walking the
    /// Shell_TrayWnd → ReBarWindow32 → MSTaskSwWClass → MSTaskListWClass window
    /// hierarchy and matching child windows by process ID (Windows 10).
    /// Falls back to <see cref="_lastTaskbarAnchorX"/> from the last direct click,
    /// then to the horizontal center of the taskbar rect (Windows 11 fallback).
    /// </summary>
    private static int FindTaskbarButtonCenterX(NativeMethods.RECT taskbarRect)
        => FindTaskbarButtonCenter(taskbarRect, out _);

    /// <summary>Returns the center of our taskbar button and sets <paramref name="buttonRect"/>
    /// to its screen rect (all zeros if not found via win32 walk).</summary>
    private static int FindTaskbarButtonCenter(NativeMethods.RECT taskbarRect, out NativeMethods.RECT buttonRect)
    {
        buttonRect = default;
        try
        {
            uint ourPid = (uint)Environment.ProcessId;
            IntPtr tray = NativeMethods.FindWindow("Shell_TrayWnd", null);
            if (tray != IntPtr.Zero)
            {
                // Windows 10: Shell_TrayWnd → ReBarWindow32 → MSTaskSwWClass → MSTaskListWClass
                IntPtr rebar = NativeMethods.FindWindowEx(tray,  IntPtr.Zero, "ReBarWindow32",   null);
                IntPtr sw    = NativeMethods.FindWindowEx(rebar != IntPtr.Zero ? rebar : tray,
                                                          IntPtr.Zero, "MSTaskSwWClass", null);
                IntPtr list  = NativeMethods.FindWindowEx(sw != IntPtr.Zero ? sw : tray,
                                                          IntPtr.Zero, "MSTaskListWClass", null);
                if (list != IntPtr.Zero)
                {
                    int found = -1;
                    NativeMethods.RECT foundRect = default;
                    NativeMethods.EnumChildWindows(list, (hwnd, _) =>
                    {
                        NativeMethods.GetWindowThreadProcessId(hwnd, out uint pid);
                        if (pid != ourPid) return true;
                        NativeMethods.GetWindowRect(hwnd, out var r);
                        foundRect = r;
                        found = (r.Left + r.Right) / 2;
                        return false; // stop enumeration
                    }, IntPtr.Zero);

                    if (found >= 0) { buttonRect = foundRect; return found; }
                }
            }
        }
        catch { /* ignore — fall through to fallbacks */ }

        // Cache from last direct click (reliable after first use)
        if (_lastTaskbarAnchorX >= 0) return _lastTaskbarAnchorX;

        // Last resort: horizontal center of the taskbar (reasonable on Win11 centered layout)
        return (taskbarRect.Left + taskbarRect.Right) / 2;
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

        try
        {
        // Render into a 2× bitmap so each final pixel is the average of 4
        // high-res samples — eliminates the jagged edges of the old approach.
        const int SS = 2;
        int ws = w * SS, hs = h * SS;

        if (_hiresBmp == null || _hiresBmp.Width != ws || _hiresBmp.Height != hs)
        {
            _hiresBmp?.Dispose();
            _hiresBmp = new Bitmap(ws, hs, System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        }

        using (var g = Graphics.FromImage(_hiresBmp))
        {
            g.Clear(Color.Transparent);
            g.SmoothingMode     = SmoothingMode.AntiAlias;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode   = PixelOffsetMode.HighQuality;
            g.ScaleTransform(SS, SS); // all DrawItem coordinates stay in 1× space

            var font = EnsureFont();

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

            // Arrow is always drawn first (lowest z-order) — never covers file icons.
            int arrowSlot = Array.FindIndex(_drawOrder,
                idx => idx < _items.Count && _items[idx].IsArrow);
            if (arrowSlot > 0)
            {
                int tmp = _drawOrder[arrowSlot];
                Array.Copy(_drawOrder, 0, _drawOrder, 1, arrowSlot);
                _drawOrder[0] = tmp;
            }

            foreach (int i in _drawOrder)
                DrawItem(g, i, font, _sf!);
        }

        // ── Downscale 2× → 1× with bicubic filtering ──────────────────────
        if (_offscreenBmp == null || _offscreenBmp.Width != w || _offscreenBmp.Height != h)
        {
            _offscreenBmp?.Dispose();
            _offscreenBmp = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        }

        using (var gOut = Graphics.FromImage(_offscreenBmp))
        {
            gOut.Clear(Color.Transparent);
            gOut.InterpolationMode = InterpolationMode.HighQualityBicubic;
            gOut.PixelOffsetMode   = PixelOffsetMode.HighQuality;
            gOut.DrawImage(_hiresBmp, 0, 0, w, h);
        }

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
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"DrawToLayeredWindow error: {ex}");
        }
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

    private void DrawItem(Graphics g, int i, Font font, StringFormat sf)
    {
        var item = _items[i];
        var iconPos = _iconPositions[i];

        // ── Entry animation offset + scale ──
        var (eDx, eDy, eScale, eAlpha) = GetEntryAnim(i);
        float entryAlpha = Math.Clamp(eAlpha, 0f, 255f);

        // Skip until item is meaningfully visible — prevents sub-pixel blips
        if (entryAlpha < 4f || eScale < 0.04f) return;

        // ── Hover scale via eased progress ──
        float t = (i < _animProgress.Length) ? EaseOutQuart(_animProgress[i]) : 0f;
        float hoverScale = 1f + t * (HoverScaleMax - 1f);

        float combinedScale = Math.Max(hoverScale * eScale, 0.01f); // never zero
        float drawSize = _iconSize * combinedScale;
        float cx = iconPos.X + eDx + _iconSize * 0.5f;
        float cy = iconPos.Y + eDy + _iconSize * 0.5f;
        float drawX = cx - drawSize * 0.5f;
        float drawY = cy - drawSize * 0.5f;

        if (item.IsArrow) { DrawArrow(g, drawX, drawY, drawSize); return; }

        // ── Text label (drawn first so icon renders on top) ──
        float textScale = 1f + t * 0.08f;
        bool ownFont = textScale > 1.005f;
        Font drawFont = ownFont
            ? new Font(font.FontFamily, _fontSize * textScale, FontStyle.Bold, GraphicsUnit.Point)
            : font;

        try
        {
            float emPx = g.DpiY * (ownFont ? _fontSize * textScale : _fontSize) / 72f;

            const float PillPadH = 8f;
            const float PillPadV = 3f;

            if (item.MeasuredTextWidth < 0f)
            {
                item.MeasuredTextWidth = g.MeasureString(item.Name, font,
                    new SizeF(float.MaxValue, 32f), sf).Width;
            }

            float rawTextW = ownFont
                ? g.MeasureString(item.Name, drawFont, new SizeF(float.MaxValue, 32f), sf).Width
                : item.MeasuredTextWidth;

            float textW = rawTextW;

            float pillW     = textW + PillPadH * 2f;
            float pillH     = emPx + PillPadV * 2f;
            float pillRight = iconPos.X + eDx - LabelGap;
            float pillLeft  = MathF.Max(0f, pillRight - pillW);
            pillW = pillRight - pillLeft;
            if (pillW >= PillPadH * 2f + 4f)
            {
                float pillTop = iconPos.Y + eDy + (_iconSize - pillH) / 2f;
                float radius  = MathF.Min(pillH / 2f - 0.5f, pillW / 2f - 0.5f);

                using var pillPath = SquirclePath(new RectangleF(pillLeft, pillTop, pillW, pillH), radius);
                int bgAlpha = (int)((190 + t * 30) * entryAlpha / 255f);
                using var pillBrush = new SolidBrush(Color.FromArgb(Math.Clamp(bgAlpha, 0, 255), 20, 20, 20));
                g.FillPath(pillBrush, pillPath);

                float textAreaW = pillW - PillPadH * 2f;
                var layoutRect = new RectangleF(pillLeft + PillPadH, pillTop + PillPadV, textAreaW, emPx + PillPadV);
                int textAlpha = Math.Clamp((int)entryAlpha, 0, 255);
                using var textBrush = new SolidBrush(Color.FromArgb(textAlpha, 255, 255, 255));
                g.DrawString(item.Name, drawFont, textBrush, layoutRect, sf);
            }
        }
        finally
        {
            if (ownFont) drawFont.Dispose();
        }

        // ── Icon (drawn after label so it sits on top) ──
        if (item.Bmp != null)
        {
            if (t > 0.01f)
            {
                var srcRect = new RectangleF(0, 0, item.Bmp.Width, item.Bmp.Height);
                (float offset, float peakAlpha)[] passes =
                {
                    (2f,  18f),
                    (4f,  14f),
                    (6f,  10f),
                    (8f,   6f),
                    (11f,  3f),
                };
                foreach (var (offset, peakAlpha) in passes)
                    DrawShadowPass(g, item.Bmp, srcRect, drawX, drawY, drawSize,
                        offset, alpha: (int)(t * peakAlpha));
            }

            if (entryAlpha >= 254f)
            {
                g.DrawImage(item.Bmp, new RectangleF(drawX, drawY, drawSize, drawSize));
            }
            else
            {
                // Apply per-item alpha during entry animation
                using var ia = MakeAlphaAttributes(entryAlpha / 255f);
                var srcRect = new RectangleF(0, 0, item.Bmp.Width, item.Bmp.Height);
                PointF[] destPts =
                {
                    new PointF(drawX,            drawY),
                    new PointF(drawX + drawSize,  drawY),
                    new PointF(drawX,             drawY + drawSize),
                };
                g.DrawImage(item.Bmp, destPts, srcRect, GraphicsUnit.Pixel, ia);
            }
        }
    }

    private static void DrawShadowPass(Graphics g, Bitmap bmp, RectangleF srcRect,
        float drawX, float drawY, float drawSize, float offset, int alpha)
    {
        if (alpha <= 0) return;
        var cm = new System.Drawing.Imaging.ColorMatrix(new float[][]
        {
            new float[] { 0, 0, 0, 0, 0 },
            new float[] { 0, 0, 0, 0, 0 },
            new float[] { 0, 0, 0, 0, 0 },
            new float[] { 0, 0, 0, alpha / 255f, 0 },
            new float[] { 0, 0, 0, 0, 1 },
        });
        using var ia = new System.Drawing.Imaging.ImageAttributes();
        ia.SetColorMatrix(cm);
        float[] offs = { -offset, 0, offset };
        foreach (float ox in offs)
        foreach (float oy in offs)
        {
            if (ox == 0f && oy == 0f) continue;
            var r = new RectangleF(drawX + ox, drawY + oy, drawSize, drawSize);
            g.DrawImage(bmp,
                new[] { r.Location, new PointF(r.Right, r.Top), new PointF(r.Left, r.Bottom) },
                srcRect, GraphicsUnit.Pixel, ia);
        }
    }

    private static System.Drawing.Imaging.ImageAttributes MakeAlphaAttributes(float alpha)
    {
        var cm = new System.Drawing.Imaging.ColorMatrix(new float[][]
        {
            new float[] { 1, 0, 0, 0, 0 },
            new float[] { 0, 1, 0, 0, 0 },
            new float[] { 0, 0, 1, 0, 0 },
            new float[] { 0, 0, 0, alpha, 0 },
            new float[] { 0, 0, 0, 0, 1 },
        });
        var ia = new System.Drawing.Imaging.ImageAttributes();
        ia.SetColorMatrix(cm);
        return ia;
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
            _hoveredIndex = idx;
            _animTimer?.Start(); // ensure running for hover animation
            Cursor = idx >= 0 ? Cursors.Hand : Cursors.Default;
            _dirty = true;   // timer will repaint; no blocking render on mouse-move thread
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
            _hoveredIndex = -1;
            Cursor = Cursors.Default;
            _dirty = true;
            _animTimer?.Start();
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

        if (_dragging) return;

        int idx = HitTest(e.Location);
        if (idx < 0 || idx >= _items.Count) return;

        var item = _items[idx];
        if (string.IsNullOrEmpty(item.FullPath)) return;

        if (e.Button == MouseButtons.Left)
        {
            LaunchItem(item);
            Close();
        }
        else if (e.Button == MouseButtons.Right)
        {
            ShowItemContextMenu(item, PointToScreen(e.Location));
        }
    }

    private void ShowItemContextMenu(FanItem item, Point screenPt)
    {
        var pidl = NativeMethods.ILCreateFromPath(item.FullPath);
        if (pidl == IntPtr.Zero) return;

        try
        {
            var iidSF = NativeMethods.IID_IShellFolder;
            int hr = NativeMethods.SHBindToParent(pidl, ref iidSF, out var psf, out var pidlChild);
            if (hr != 0) return;

            try
            {
                // GetUIObjectOf supports IContextMenu (base), not IContextMenu2 directly
                var iidCM = NativeMethods.IID_IContextMenu;
                var apidl = new[] { pidlChild };
                hr = psf.GetUIObjectOf(Handle, 1, apidl, ref iidCM, IntPtr.Zero, out var cmObj);
                if (hr != 0) return;

                var pcm = (NativeMethods.IContextMenu)cmObj;

                // QI for IContextMenu2 to support owner-draw submenus (Send to, Open with…)
                NativeMethods.IContextMenu2? pcm2 = null;
                var punk = Marshal.GetIUnknownForObject(cmObj);
                try
                {
                    var iid2 = NativeMethods.IID_IContextMenu2;
                    if (Marshal.QueryInterface(punk, ref iid2, out var p2) == 0)
                    {
                        pcm2 = (NativeMethods.IContextMenu2)Marshal.GetObjectForIUnknown(p2);
                        Marshal.Release(p2);
                    }
                }
                finally { Marshal.Release(punk); }

                var hMenu = NativeMethods.CreatePopupMenu();
                if (hMenu == IntPtr.Zero) return;

                try
                {
                    if (pcm2 != null)
                        pcm2.QueryContextMenu(hMenu, 0, NativeMethods.ID_CMD_FIRST, NativeMethods.ID_CMD_LAST, NativeMethods.CMF_EXPLORE);
                    else
                        pcm.QueryContextMenu (hMenu, 0, NativeMethods.ID_CMD_FIRST, NativeMethods.ID_CMD_LAST, NativeMethods.CMF_EXPLORE);

                    _contextMenuOpen    = true;
                    _activeContextMenu2 = pcm2;

                    NativeMethods.SetForegroundWindow(Handle);

                    int cmd = NativeMethods.TrackPopupMenu(hMenu,
                        NativeMethods.TPM_RETURNCMD | NativeMethods.TPM_RIGHTBUTTON,
                        screenPt.X, screenPt.Y, 0, Handle, IntPtr.Zero);

                    NativeMethods.PostMessage(Handle, NativeMethods.WM_NULL, IntPtr.Zero, IntPtr.Zero);

                    _activeContextMenu2 = null;
                    _contextMenuOpen    = false;

                    if (cmd >= NativeMethods.ID_CMD_FIRST)
                    {
                        if (IsRenameVerb(pcm, pcm2, cmd - NativeMethods.ID_CMD_FIRST))
                        {
                            // The shell Rename verb only works inside Explorer (inline
                            // edit).  Show our own dialog and call File/Directory.Move.
                            // Keep _contextMenuOpen = true so the Deactivate handler
                            // does not close the fan while the rename dialog is modal.
                            _contextMenuOpen = true;
                            try   { HandleRename(item, screenPt); }
                            finally { _contextMenuOpen = false; }
                        }
                        else
                        {
                            var ici = new NativeMethods.CMINVOKECOMMANDINFO
                            {
                                cbSize = Marshal.SizeOf<NativeMethods.CMINVOKECOMMANDINFO>(),
                                hwnd   = Handle,
                                lpVerb = (IntPtr)(cmd - NativeMethods.ID_CMD_FIRST),
                                nShow  = NativeMethods.SW_SHOWNORMAL,
                            };
                            if (pcm2 != null) pcm2.InvokeCommand(ref ici);
                            else              pcm.InvokeCommand(ref ici);
                            FileSystemModified?.Invoke(this, EventArgs.Empty);
                        }
                    }
                    Close();
                }
                finally
                {
                    NativeMethods.DestroyMenu(hMenu);
                    if (pcm2 != null) Marshal.ReleaseComObject(pcm2);
                    Marshal.ReleaseComObject(pcm);
                }
            }
            finally
            {
                Marshal.ReleaseComObject(psf);
            }
        }
        finally
        {
            NativeMethods.ILFree(pidl);
        }
    }

    // Returns true when the selected context-menu command is the shell "rename" verb.
    private static bool IsRenameVerb(NativeMethods.IContextMenu pcm,
                                     NativeMethods.IContextMenu2? pcm2,
                                     int cmdOffset)
    {
        IntPtr buf = Marshal.AllocHGlobal(512); // 256 WCHARs
        try
        {
            int hr = pcm2 != null
                ? pcm2.GetCommandString((UIntPtr)cmdOffset, NativeMethods.GCS_VERBW, IntPtr.Zero, buf, 256)
                : pcm .GetCommandString((UIntPtr)cmdOffset, NativeMethods.GCS_VERBW, IntPtr.Zero, buf, 256);
            if (hr >= 0)
            {
                string verb = Marshal.PtrToStringUni(buf) ?? string.Empty;
                return verb.Equals("rename", StringComparison.OrdinalIgnoreCase);
            }
        }
        catch { /* GetCommandString can throw for some verbs */ }
        finally { Marshal.FreeHGlobal(buf); }
        return false;
    }

    private void HandleRename(FanItem item, Point screenPt)
    {
        string? newName = ShowRenameDialog(item.Name, screenPt);
        if (newName == null || newName.Equals(item.Name, StringComparison.Ordinal))
            return;

        string dir  = Path.GetDirectoryName(item.FullPath) ?? string.Empty;
        string dest = Path.Combine(dir, newName);
        try
        {
            if (item.IsDirectory) Directory.Move(item.FullPath, dest);
            else                  File.Move(item.FullPath, dest);
            FileSystemModified?.Invoke(this, EventArgs.Empty);
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Rename failed:\n{ex.Message}", "Fan Folder",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private static string? ShowRenameDialog(string currentName, Point anchor)
    {
        using var f = new Form();
        f.Text             = "Rename";
        f.FormBorderStyle  = FormBorderStyle.FixedDialog;
        f.MaximizeBox      = false;
        f.MinimizeBox      = false;
        f.TopMost          = true;
        f.StartPosition    = FormStartPosition.Manual;
        f.ClientSize       = new Size(360, 84);
        f.Location         = new Point(anchor.X - f.ClientSize.Width / 2,
                                       anchor.Y - f.ClientSize.Height / 2);

        var lbl = new Label  { Text = "New name:", AutoSize = true,
                               Location = new Point(10, 14) };
        var tb  = new TextBox{ Text = currentName,
                               Location = new Point(80, 10), Width = 268 };
        var ok  = new Button { Text = "OK",     DialogResult = DialogResult.OK,
                               Location = new Point(196, 46), Width = 76 };
        var cancel = new Button { Text = "Cancel", DialogResult = DialogResult.Cancel,
                                  Location = new Point(278, 46), Width = 76 };

        f.Controls.AddRange([lbl, tb, ok, cancel]);
        f.AcceptButton = ok;
        f.CancelButton = cancel;
        f.Shown += (_, _) => { tb.SelectAll(); tb.Focus(); };

        if (f.ShowDialog() == DialogResult.OK)
        {
            string name = tb.Text.Trim();
            return name.Length > 0 ? name : null;
        }
        return null;
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

    protected override bool ShowWithoutActivation => true;

    /// <summary>
    /// For areas with items: return HTCLIENT so mouse events are routed here.
    /// For empty/transparent areas: return HTTRANSPARENT so clicks pass through
    /// to the window below. The global mouse hook in MainHiddenForm handles closing.
    /// </summary>
    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST     = 0x0084;
        const int WM_CONTEXTMENU   = 0x007B;
        const int HTCLIENT         = 1;
        const int HTTRANSPARENT    = -1;
        const int WM_INITMENUPOPUP = 0x0117;
        const int WM_DRAWITEM      = 0x002B;
        const int WM_MEASUREITEM   = 0x002C;

        // Forward menu messages to IContextMenu2 so owner-draw submenus
        // (Send to, Open with, etc.) render and populate correctly.
        if (_activeContextMenu2 != null &&
            (m.Msg == WM_INITMENUPOPUP || m.Msg == WM_DRAWITEM || m.Msg == WM_MEASUREITEM))
        {
            _activeContextMenu2.HandleMenuMsg((uint)m.Msg, m.WParam, m.LParam);
            m.Result = IntPtr.Zero;
            return;
        }

        // WM_RBUTTONUP: handle right-click directly at Win32 level (more reliable than
        // OnMouseClick on WS_EX_NOACTIVATE layered windows).
        const int WM_RBUTTONUP = 0x0205;
        if (m.Msg == WM_RBUTTONUP)
        {
            long lp2 = m.LParam.ToInt64();
            var localPt2 = new Point((short)(lp2 & 0xFFFF), (short)((lp2 >> 16) & 0xFFFF));
            int idx2 = HitTest(localPt2);
            if (idx2 >= 0 && idx2 < _items.Count && !string.IsNullOrEmpty(_items[idx2].FullPath))
                ShowItemContextMenu(_items[idx2], PointToScreen(localPt2));
            return;
        }

        // Use long arithmetic so we don't overflow on 64-bit.
        if (m.Msg == WM_CONTEXTMENU)
        {
            long lp = m.LParam.ToInt64();
            Point screenPt;
            if ((int)lp == -1)
            {
                screenPt = Cursor.Position;
            }
            else
            {
                screenPt = new Point((short)(lp & 0xFFFF), (short)((lp >> 16) & 0xFFFF));
            }

            var localPt = PointToClient(screenPt);
            int idx = HitTest(localPt);
            if (idx >= 0 && idx < _items.Count && !string.IsNullOrEmpty(_items[idx].FullPath))
            {
                ShowItemContextMenu(_items[idx], screenPt);
            }
            return;
        }

        if (m.Msg == WM_NCHITTEST)
        {
            // The window region (SetWindowRgn) already excludes transparent
            // gaps, so most empty-area mouse moves never reach us.  For the
            // small padded margin around each icon we still need to verify
            // the cursor is truly over an icon/label before claiming HTCLIENT.
            int screenX = (short)(m.LParam.ToInt32() & 0xFFFF);
            int screenY = (short)((m.LParam.ToInt32() >> 16) & 0xFFFF);
            var client  = PointToClient(new Point(screenX, screenY));
            m.Result    = HitTest(client) >= 0
                ? (IntPtr)HTCLIENT
                : (IntPtr)HTTRANSPARENT;
            return;
        }
        base.WndProc(ref m);
    }

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= 0x00000080; // WS_EX_TOOLWINDOW  – hide from Alt+Tab
            cp.ExStyle |= 0x00080000; // WS_EX_LAYERED     – per-pixel alpha via UpdateLayeredWindow
            cp.ExStyle |= 0x08000000; // WS_EX_NOACTIVATE  – never steal focus from other windows
            return cp;
        }
    }

    // ═════════════════════════════════════════════════════════
    //  Cleanup
    // ═════════════════════════════════════════════════════════

    protected override void Dispose(bool disposing)
    {
        if (disposing && !IsDisposed)
        {
            if (!_iconLoadCts.IsCancellationRequested)
                _iconLoadCts.Cancel();
            _iconLoadCts.Dispose();
            _animTimer?.Stop();
            _animTimer?.Dispose();
            _font?.Dispose();
            _sf?.Dispose();
            _offscreenBmp?.Dispose();
            _hiresBmp?.Dispose();
            foreach (var item in _items)
            {
                item.Bmp?.Dispose();
                item.Icon?.Dispose();
            }
        }
        base.Dispose(disposing);
    }
}
