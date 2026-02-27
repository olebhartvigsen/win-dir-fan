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
    /// given file.  Falls back to WinForms DoDragDrop if the Shell data
    /// object creation fails.
    /// </summary>
    public static DragDropEffects DoDragDrop(
        Control source, string filePath, DragDropEffects allowedEffects)
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
