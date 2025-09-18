import sys
from pathlib import Path
from io import BytesIO

from PySide6 import QtCore, QtGui

from PIL import Image, ImageDraw, ImageFont

if sys.platform == "win32":
    try:
        import win32con  # noqa
        import win32api  # noqa
        import win32gui  # noqa
        import win32com.client  # noqa
    except Exception:
        pass


class ThumbnailWorker(QtCore.QThread):
    thumbnailReady = QtCore.Signal(object, object)  # path, png bytes

    def __init__(self, parent=None):
        super().__init__(parent)
        self._files = []
        self._stopped = False

    def set_files(self, files):
        self._files = list(files)

    def stop(self):
        self._stopped = True
        try:
            # politely ask the thread to quit and wait briefly
            if self.isRunning():
                self.quit()
                self.wait(500)
        except Exception:
            pass

    def run(self):
        for p in self._files:
            if self._stopped:
                break
            try:
                png_bytes = self.generate_thumbnail(p)
                # png_bytes may be None for non-image files; GUI thread will request system icon
                self.thumbnailReady.emit(str(p), png_bytes)
            except Exception:
                # emit a simple empty PNG as fallback
                img = Image.new("RGBA", (96, 96), (200, 200, 200, 255))
                bio = BytesIO()
                img.save(bio, format="PNG")
                self.thumbnailReady.emit(str(p), bio.getvalue())

    def generate_thumbnail(self, path: Path, size=96) -> QtGui.QPixmap:
        path = Path(path)
        if path.suffix.lower() in [".png", ".jpg", ".jpeg", ".bmp", ".gif"]:
            img = Image.open(path)
            img.thumbnail((size, size))
            bio = BytesIO()
            img.save(bio, format="PNG")
            return bio.getvalue()

        # For non-image files, return None so the GUI thread can request the
        # platform-native icon via QFileIconProvider (safer to do in GUI thread).
        # Attempting to use COM or GUI APIs here in a worker thread can be
        # unreliable, so defer to the main thread.
        return None
