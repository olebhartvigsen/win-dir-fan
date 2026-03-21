using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace FanFolderApp;

/// <summary>
/// Uses the Windows Shell to create a proper drag-and-drop data object
/// identical to what File Explorer produces.  This ensures applications
/// like Word handle dropped files correctly (e.g., rendering SVG content
/// instead of showing a file icon).
/// </summary>
internal static class ShellDragHelper
{
    private static readonly Guid IID_IDataObject =
        new("0000010e-0000-0000-C000-000000000046");

    /// <summary>
    /// Performs an OLE drag-and-drop using a Shell data object for the
    /// given file with the actual file icon as the drag image.
    /// </summary>
    public static DragDropEffects DoDragDrop(
        Control source, string filePath, DragDropEffects allowedEffects,
        Icon? dragIcon = null)
    {
        IntPtr pidlFull = NativeMethods.ILCreateFromPath(filePath);
        if (pidlFull == IntPtr.Zero)
            return FallbackDragDrop(source, filePath, allowedEffects);

        IntPtr pidlFolder = IntPtr.Zero;
        IntPtr pDataObj = IntPtr.Zero;

        try
        {
            // Split the full PIDL into parent-folder + relative child
            pidlFolder = NativeMethods.ILClone(pidlFull);
            NativeMethods.ILRemoveLastID(pidlFolder);

            IntPtr pidlChild = NativeMethods.ILFindLastID(pidlFull);
            IntPtr[] children = [pidlChild];

            Guid iid = IID_IDataObject;
            int hr = NativeMethods.SHCreateDataObject(
                pidlFolder, 1, children, IntPtr.Zero,
                ref iid, out pDataObj);

            if (hr != 0 || pDataObj == IntPtr.Zero)
                return FallbackDragDrop(source, filePath, allowedEffects);

            // Set custom drag image using the file's actual icon
            if (dragIcon != null)
                SetDragImage(pDataObj, dragIcon);

            var dropSource = new DropSourceImpl();
            NativeMethods.DoDragDrop(
                pDataObj, dropSource,
                (int)allowedEffects, out int effect);

            return (DragDropEffects)effect;
        }
        finally
        {
            if (pDataObj != IntPtr.Zero) Marshal.Release(pDataObj);
            if (pidlFolder != IntPtr.Zero) NativeMethods.ILFree(pidlFolder);
            NativeMethods.ILFree(pidlFull);
        }
    }

    // ─── Drag Image ──────────────────────────────────────────────────

    private static void SetDragImage(IntPtr pDataObj, Icon icon)
    {
        try
        {
            // Convert icon to a 32-bit ARGB bitmap (premultiplied alpha
            // is required by IDragSourceHelper for proper transparency).
            using var bmp = icon.ToBitmap();
            int w = bmp.Width;
            int h = bmp.Height;

            // Create a premultiplied-alpha copy
            using var pma = new Bitmap(w, h, PixelFormat.Format32bppPArgb);
            using (var g = Graphics.FromImage(pma))
            {
                g.DrawImage(bmp, 0, 0, w, h);
            }

            IntPtr hBmp = pma.GetHbitmap(Color.Empty); // preserves alpha
            try
            {
                var shdi = new NativeMethods.SHDRAGIMAGE
                {
                    sizeDragImage = new NativeMethods.SIZE { cx = w, cy = h },
                    ptOffset = new Point(w / 2, h / 2),
                    hbmpDragImage = hBmp,
                    crColorKey = 0  // no colour key — use alpha
                };

                var helper = (NativeMethods.IDragSourceHelper)
                    new NativeMethods.DragDropHelper();

                // Wrap the raw COM pointer in a managed object for the interface call
                object dataObj = Marshal.GetObjectForIUnknown(pDataObj);
                int hr = helper.InitializeFromBitmap(ref shdi, dataObj);

                // If successful, the helper takes ownership of the HBITMAP
                if (hr != 0)
                    NativeMethods.DeleteObject(hBmp);

                // Release our extra COM ref
                Marshal.ReleaseComObject(dataObj);
                Marshal.ReleaseComObject(helper);
            }
            catch
            {
                NativeMethods.DeleteObject(hBmp);
            }
        }
        catch
        {
            // Non-critical: drag still works, just with default icon
        }
    }

    // ─── Fallback ────────────────────────────────────────────────────

    private static DragDropEffects FallbackDragDrop(
        Control source, string filePath, DragDropEffects allowedEffects)
    {
        var data = new DataObject(DataFormats.FileDrop,
            new[] { filePath });
        return source.DoDragDrop(data, allowedEffects);
    }

    // ─── IDropSource implementation ──────────────────────────────────

    private sealed class DropSourceImpl : NativeMethods.IDropSource
    {
        private const int S_OK = 0;
        private const int DRAGDROP_S_DROP = 0x00040100;
        private const int DRAGDROP_S_CANCEL = 0x00040101;
        private const int DRAGDROP_S_USEDEFAULTCURSORS = 0x00040102;
        private const int MK_LBUTTON = 0x0001;

        public int QueryContinueDrag(bool fEscapePressed, int grfKeyState)
        {
            if (fEscapePressed) return DRAGDROP_S_CANCEL;
            if ((grfKeyState & MK_LBUTTON) == 0) return DRAGDROP_S_DROP;
            return S_OK;
        }

        public int GiveFeedback(int dwEffect)
        {
            return DRAGDROP_S_USEDEFAULTCURSORS;
        }
    }
}
