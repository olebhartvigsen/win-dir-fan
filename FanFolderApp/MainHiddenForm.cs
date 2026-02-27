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
    private bool _suppressNextActivation;
    private DateTime _lastFanCloseTime = DateTime.MinValue;
    private Icon? _stackIcon;
    private Icon? _arrowIcon;

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

    // ─── Taskbar Icon ────────────────────────────────────────

    private void SetTaskbarIcon()
    {
        _stackIcon = CreateStackIcon(256);
        _arrowIcon = CreateArrowIcon(256);
        Icon = _stackIcon;
    }

    /// <summary>
    /// Draws a macOS-style "stack" icon: two document pages fanned behind
    /// a folder, giving the appearance of files stacked on each other.
    /// </summary>
    private static Icon CreateStackIcon(int size)
    {
        using var bmp = new Bitmap(size, size, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(bmp);
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        g.Clear(Color.Transparent);

        float pad = size * 0.06f;
        float w = size - pad * 2;
        float h = size - pad * 2;

        // ── Folder (drawn first, behind documents) ───────────
        {
            float fW = w * 0.88f;
            float fH = h * 0.48f;
            float fx = pad + (w - fW) / 2;
            float fy = pad + h - fH - h * 0.04f;
            float r = size * 0.04f; // corner radius
            float tabW = fW * 0.35f;
            float tabH = fH * 0.16f;

            using var folderPath = new System.Drawing.Drawing2D.GraphicsPath();
            // Tab
            folderPath.AddArc(fx, fy - tabH, r, r, 180, 90);
            folderPath.AddLine(fx + r, fy - tabH, fx + tabW - r, fy - tabH);
            folderPath.AddArc(fx + tabW - r, fy - tabH, r, r, 270, 90);
            folderPath.AddLine(fx + tabW, fy - tabH + r, fx + tabW + tabH * 0.6f, fy);
            // Body
            folderPath.AddLine(fx + tabW + tabH * 0.6f, fy, fx + fW - r, fy);
            folderPath.AddArc(fx + fW - r, fy, r, r, 270, 90);
            folderPath.AddLine(fx + fW, fy + r, fx + fW, fy + fH - r);
            folderPath.AddArc(fx + fW - r, fy + fH - r, r, r, 0, 90);
            folderPath.AddLine(fx + fW - r, fy + fH, fx + r, fy + fH);
            folderPath.AddArc(fx, fy + fH - r, r, r, 90, 90);
            folderPath.CloseFigure();

            // Shadow
            using var shadow = new SolidBrush(Color.FromArgb(50, 0, 0, 0));
            g.TranslateTransform(2, 4);
            g.FillPath(shadow, folderPath);
            g.TranslateTransform(-2, -4);

            // Gradient fill
            using var grad = new System.Drawing.Drawing2D.LinearGradientBrush(
                new PointF(fx, fy - tabH), new PointF(fx, fy + fH),
                Color.FromArgb(100, 160, 235), Color.FromArgb(60, 120, 200));
            g.FillPath(grad, folderPath);

            using var pen = new Pen(Color.FromArgb(50, 95, 170), size * 0.014f);
            g.DrawPath(pen, folderPath);
        }

        // ── Back document (tilted right, on top of folder) ───
        {
            float docW = w * 0.52f;
            float docH = h * 0.62f;
            float cx = pad + w * 0.55f;
            float cy = pad + h * 0.30f;

            using var mx = new System.Drawing.Drawing2D.Matrix();
            mx.RotateAt(12f, new PointF(cx, cy));
            g.Transform = mx;

            var r = new RectangleF(cx - docW / 2, cy - docH / 2 + h * 0.02f, docW, docH);
            float corner = size * 0.04f;
            float fold = size * 0.09f;

            using var path = RoundedDocPath(r, corner, fold);
            using var shadow = new SolidBrush(Color.FromArgb(60, 0, 0, 0));
            g.TranslateTransform(2, 3);
            g.FillPath(shadow, path);
            g.TranslateTransform(-2, -3);

            using var fill = new SolidBrush(Color.FromArgb(245, 245, 250));
            using var pen = new Pen(Color.FromArgb(100, 115, 140), size * 0.016f);
            g.FillPath(fill, path);
            g.DrawPath(pen, path);

            // Lines on document
            using var linePen = new Pen(Color.FromArgb(140, 155, 175), size * 0.016f);
            float lx = r.X + r.Width * 0.15f;
            float lw = r.Width * 0.7f;
            for (int i = 0; i < 3; i++)
            {
                float ly = r.Y + r.Height * (0.40f + i * 0.14f);
                g.DrawLine(linePen, lx, ly, lx + lw, ly);
            }

            g.ResetTransform();
        }

        // ── Front document (tilted left, topmost) ────────────
        {
            float docW = w * 0.52f;
            float docH = h * 0.62f;
            float cx = pad + w * 0.42f;
            float cy = pad + h * 0.28f;

            using var mx = new System.Drawing.Drawing2D.Matrix();
            mx.RotateAt(-8f, new PointF(cx, cy));
            g.Transform = mx;

            var r = new RectangleF(cx - docW / 2, cy - docH / 2 + h * 0.02f, docW, docH);
            float corner = size * 0.04f;
            float fold = size * 0.09f;

            using var path = RoundedDocPath(r, corner, fold);
            using var shadow = new SolidBrush(Color.FromArgb(60, 0, 0, 0));
            g.TranslateTransform(2, 3);
            g.FillPath(shadow, path);
            g.TranslateTransform(-2, -3);

            using var fill = new SolidBrush(Color.White);
            using var pen = new Pen(Color.FromArgb(90, 105, 130), size * 0.016f);
            g.FillPath(fill, path);
            g.DrawPath(pen, path);

            using var linePen = new Pen(Color.FromArgb(130, 145, 165), size * 0.016f);
            float lx = r.X + r.Width * 0.15f;
            float lw = r.Width * 0.7f;
            for (int i = 0; i < 3; i++)
            {
                float ly = r.Y + r.Height * (0.40f + i * 0.14f);
                g.DrawLine(linePen, lx, ly, lx + lw, ly);
            }

            g.ResetTransform();
        }

        var handle = bmp.GetHicon();
        return Icon.FromHandle(handle);
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

        var handle = bmp.GetHicon();
        return Icon.FromHandle(handle);
    }

    // ─── Activation / Toggle ─────────────────────────────────

    protected override void WndProc(ref Message m)
    {
        const int WM_SYSCOMMAND = 0x0112;
        const int SC_RESTORE = 0xF120;

        // Taskbar click fires SC_RESTORE on a minimized window
        if (m.Msg == WM_SYSCOMMAND &&
            (m.WParam.ToInt32() & 0xFFF0) == SC_RESTORE)
        {
            ToggleFan();
            // Keep the form minimized
            WindowState = FormWindowState.Minimized;
            return; // swallow
        }

        base.WndProc(ref m);
    }

    protected override void OnActivated(EventArgs e)
    {
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
        CloseFan(); // clean up any previous instance

        _fanForm = new FanForm(_folderPath);
        _fanForm.FormClosed += (_, _) =>
        {
            _fanForm = null;
            Icon = _stackIcon;
        };
        _fanForm.Deactivate += (_, _) =>
        {
            // Dismiss when the user clicks outside
            _suppressNextActivation = true;
            _fanForm?.Close();
        };
        _fanForm.Show();
        Icon = _arrowIcon;
    }

    private void CloseFan()
    {
        if (_fanForm != null && !_fanForm.IsDisposed)
        {
            _fanForm.Close();
            _fanForm = null;
        }
        _lastFanCloseTime = DateTime.UtcNow;
        Icon = _stackIcon;
    }

    // ─── Cleanup ─────────────────────────────────────────────

    protected override void Dispose(bool disposing)
    {
        if (disposing)
            CloseFan();

        base.Dispose(disposing);
    }
}
