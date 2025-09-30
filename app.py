import sys
import os
from pathlib import Path
import platform
import logging
import time

# Ensure logging is configured before importing other modules so their import-time logs are captured.
LOG_PATH = Path(__file__).parent / "debug.log"
# Create a root logger with rotating capability fallback if file grows large later (placeholder now)
logging.basicConfig(
    filename=LOG_PATH,
    level=logging.DEBUG,
    format='%(asctime)s %(levelname)s [%(name)s] %(message)s'
)
logger = logging.getLogger("win-dir-fan.app")

# Redirect uncaught exceptions
def _excepthook(exc_type, exc_value, exc_tb):
    logger.exception("Uncaught exception", exc_info=(exc_type, exc_value, exc_tb))
    # still print to stderr for visibility
    import traceback
    traceback.print_exception(exc_type, exc_value, exc_tb)

sys.excepthook = _excepthook

# Optional: capture Qt debug output by installing a message handler if available
try:
    from PySide6 import QtCore  # import early for handler registration
    def _qt_message_handler(mode, ctx, message):  # type: ignore[unused-argument]
        lvl = logging.DEBUG
        if mode == QtCore.QtMsgType.QtWarningMsg:  # type: ignore[attr-defined]
            lvl = logging.WARNING
        elif mode == QtCore.QtMsgType.QtCriticalMsg:  # type: ignore[attr-defined]
            lvl = logging.CRITICAL
        elif mode == QtCore.QtMsgType.QtFatalMsg:  # type: ignore[attr-defined]
            lvl = logging.FATAL
        logging.getLogger("qt").log(lvl, message)
    try:
        QtCore.qInstallMessageHandler(_qt_message_handler)  # type: ignore[attr-defined]
    except Exception:
        pass
except Exception:
    pass

# Redirect stdout/stderr to the log file (append mode) besides console.
class _StreamToLogger:
    def __init__(self, level):
        self.level = level
        self._buffer = ''
    def write(self, msg):
        try:
            msg = str(msg)
            if msg.endswith('\n'):
                self._buffer += msg.rstrip('\n')
                if self._buffer:
                    logging.getLogger("stdout").log(self.level, self._buffer)
                self._buffer = ''
            else:
                self._buffer += msg
        except Exception:
            pass
    def flush(self):
        try:
            if self._buffer:
                logging.getLogger("stdout").log(self.level, self._buffer)
                self._buffer = ''
        except Exception:
            pass

if os.environ.get('WIN_DIR_FAN_REDIRECT_STD', '1') == '1':
    try:
        sys.stdout = _StreamToLogger(logging.INFO)  # type: ignore
        sys.stderr = _StreamToLogger(logging.ERROR)  # type: ignore
    except Exception:
        pass

from PySide6 import QtGui, QtWidgets  # after logging setup

from fan_widget import FanWindow

# Existing code (with logging enhancements integrated below)

class TaskbarResidentApp(QtWidgets.QApplication):
    """Resident app that shows a taskbar icon without a visible main window.

    Clicking the taskbar icon will toggle the fan popup.
    """

    def __init__(self, argv, directory: Path, max_items: int = 15):
        super().__init__(argv)
        self.directory = directory
        self.max_items = int(max_items)

        # Main window that owns the taskbar button. We keep it minimized & transparent so the user never
        # sees it, but Windows still provides a taskbar button for activation.
        self.main = QtWidgets.QMainWindow()
        self.main.setWindowTitle("Windows Dir Fan")
        # Keep size modest but we will keep it minimized & fully transparent to avoid user distraction
        try:
            self.main.resize(320, 240)
        except Exception:
            pass
        # Ensure standard window flags (no tool window) so it appears in taskbar.
        try:
            self.main.setWindowFlags(QtCore.Qt.Window | QtCore.Qt.WindowMinimizeButtonHint | QtCore.Qt.CustomizeWindowHint)  # type: ignore[attr-defined]
        except Exception:
            pass
        # Set an explicit AppUserModelID so the taskbar button is stable/grouped (Windows only).
        if sys.platform.startswith('win'):
            try:
                import ctypes  # type: ignore
                ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("win-dir-fan.app")  # type: ignore[attr-defined]
            except Exception:
                logging.exception('Failed to set AppUserModelID')
        # Initialize multi-size icon generation & runtime watcher
        try:
            self._base_dir = Path(__file__).parent
            self._png_icon_path = self._base_dir / 'app icon.png'
            self._ico_path = self._base_dir / 'app.ico'
            self._icon_last_mtime = 0.0
            self._apply_app_icon(initial=True)
            self._init_icon_refresh_support()
        except Exception:
            logging.exception('Icon initialization failed')
        # Start fully transparent, then show minimized immediately so no centered flash occurs.
        try:
            self.main.setWindowOpacity(0.0)
        except Exception:
            pass
        try:
            # showMinimized ensures taskbar button without flashing the window at center
            self.main.showMinimized()
        except Exception:
            self.main.show()
        logging.debug('Main window created minimized & transparent to avoid visual flash')

        # Install an event filter to keep it minimized / hidden if Windows attempts to restore it.
        self.main.installEventFilter(self)

        try:
            self._startup_time = QtCore.QDateTime.currentMSecsSinceEpoch()
        except Exception:
            self._startup_time = 0

        self.main.installEventFilter(self)

        self.fan = None
        self._last_show_from_taskbar = False
        self._last_activation_time = 0

        # Log startup context
        try:
            logging.info(f'Startup directory={self.directory} max_items={self.max_items} platform={platform.platform()} python={platform.python_version()}')
        except Exception:
            pass

    def eventFilter(self, watched, event):
        try:
            etype = event.type()
        except Exception:
            etype = None
        logging.debug(f'eventFilter watched={watched} event_type={etype}')
        # Use getattr with fallback numeric event ids to avoid attribute errors in some PySide builds
        _EV_ACTIVATE = getattr(QtCore.QEvent, 'WindowActivate', 24)
        _EV_STATE = getattr(QtCore.QEvent, 'WindowStateChange', 105)
        if watched is self.main and etype in (_EV_ACTIVATE, _EV_STATE):
            now = QtCore.QDateTime.currentMSecsSinceEpoch()
            if self._startup_time and now - self._startup_time < 300:  # Reduced from 800ms to 300ms
                logging.debug('Activation ignored due to startup grace period')
                return True
            if now - self._last_activation_time < 150:  # Reduced from 250ms to 150ms
                logging.debug('Activation ignored due to debounce')
                return True
            self._last_activation_time = now
            try:
                screen = QtWidgets.QApplication.primaryScreen()
                screen_geom = screen.geometry()
                avail = screen.availableGeometry()
                taskbar_height = screen_geom.height() - avail.height()
                taskbar_width = screen_geom.width() - avail.width()
                pos = QtGui.QCursor.pos()
                in_taskbar = False
                if taskbar_height > 0:
                    if avail.top() > screen_geom.top():
                        in_taskbar = pos.y() <= (screen_geom.top() + taskbar_height + 16)
                    else:
                        in_taskbar = pos.y() >= (screen_geom.bottom() - taskbar_height - 16)
                elif taskbar_width > 0:
                    if avail.left() > screen_geom.left():
                        in_taskbar = pos.x() <= (screen_geom.left() + taskbar_width + 16)
                    else:
                        in_taskbar = pos.x() >= (screen_geom.right() - taskbar_width - 16)
                else:
                    in_taskbar = True
                logging.debug(f'Activation cursor pos={pos.x()},{pos.y()} in_taskbar={in_taskbar} taskbar_h={taskbar_height} taskbar_w={taskbar_width}')
                if not in_taskbar:
                    logging.debug('Activation ignored because cursor not in taskbar area')
                    return True
            except Exception:
                logging.exception('Failed taskbar heuristic check; proceeding')
            pos = QtGui.QCursor.pos()

            # Check if fan is already visible - if so, hide it (toggle behavior)
            if self.fan and self.fan.isVisible():
                logging.debug('Fan already visible; hiding it (toggle behavior)')
                self.fan.hide()
                return True
                
            # Fan is not visible, so show it
            if not self.fan:
                logging.debug('Creating FanWindow instance')
                self.fan = FanWindow(self.directory, max_items=self.max_items)
                
            logging.debug('Refreshing fan contents')
            self.fan.refresh()
            
            # Mark that this was opened from taskbar for better focus behavior
            self.fan.set_taskbar_opened(True)
            
            try:
                self.fan.set_anchor_x(pos.x())
            except Exception:
                pass
            self.fan.move(pos.x() - self.fan.width() // 2, pos.y() - self.fan.height() - 8)
            logging.debug(f'Showing fan at {self.fan.x()},{self.fan.y()}')
            self.fan.show()
            try:
                self.fan.raise_()
            except Exception:
                pass
            try:
                self.fan.activateWindow()
            except Exception:
                pass
            self._last_show_from_taskbar = True

            logging.debug('Handled activation; showing/hiding fan')
            QtCore.QTimer.singleShot(1, lambda: None)
            return True

        return super().eventFilter(watched, event)

    # ---------------- Icon Handling (multi-size + runtime refresh) -----------------
    def _generate_multi_size_ico(self, png_path: Path, ico_path: Path):
        """Generate multi-size ICO (16/32/48/64/128/256). Uses Pillow if available, else falls back to largest-only via Qt."""
        sizes = [16,32,48,64,128,256]
        try:
            try:
                from PIL import Image  # type: ignore
                img = Image.open(str(png_path))
                # Pillow multi-size saving
                img.save(str(ico_path), format='ICO', sizes=[(s, s) for s in sizes])
                logging.info('Generated multi-size ICO via Pillow: %s', ico_path)
                return True
            except Exception:
                pm = QtGui.QPixmap(str(png_path))
                if pm.isNull():
                    return False
                pm_largest = pm.scaled(256,256,QtCore.Qt.AspectRatioMode.KeepAspectRatio, QtCore.Qt.TransformationMode.SmoothTransformation)
                if pm_largest.save(str(ico_path), 'ICO'):
                    logging.info('Generated single-size ICO via Qt: %s', ico_path)
                    return True
                # Fallback try QImage variant
                try:
                    pm_largest.toImage().save(str(ico_path), b'ICO')  # type: ignore[arg-type]
                    logging.info('Generated single-size ICO via Qt (QImage): %s', ico_path)
                    return True
                except Exception:
                    return False
        except Exception:
            logging.exception('ICO generation failed')
            return False

    def _apply_app_icon(self, initial: bool=False):
        try:
            png = getattr(self, '_png_icon_path', None)
            ico = getattr(self, '_ico_path', None)
            if not png or not ico:
                return
            chosen = None
            if png.exists():
                png_mtime = png.stat().st_mtime
                regen_needed = (not ico.exists()) or png_mtime > (ico.stat().st_mtime + 0.1)
                if regen_needed:
                    ok = self._generate_multi_size_ico(png, ico)
                    if not ok:
                        logging.warning('ICO regeneration failed; continuing with PNG only')
                self._icon_last_mtime = png_mtime
            if ico.exists():
                chosen = QtGui.QIcon(str(ico))
            elif png.exists():
                pm = QtGui.QPixmap(str(png))
                if not pm.isNull():
                    chosen = QtGui.QIcon(pm)
            if chosen:
                self.setWindowIcon(chosen)
                self.main.setWindowIcon(chosen)
                logging.debug('Applied application icon (initial=%s)', initial)
        except Exception:
            logging.exception('Apply icon failed')

    def _init_icon_refresh_support(self):
        try:
            # Timer-based polling watcher (every 2s) + manual shortcut (Ctrl+Shift+R)
            self._icon_watch_timer = QtCore.QTimer(self)
            self._icon_watch_timer.setInterval(2000)
            self._icon_watch_timer.timeout.connect(self._check_icon_update)  # type: ignore
            self._icon_watch_timer.start()
            shortcut = QtGui.QShortcut(QtGui.QKeySequence('Ctrl+Shift+R'), self.main)
            shortcut.activated.connect(self._manual_icon_refresh)  # type: ignore
            logging.info('Icon refresh watcher started (Ctrl+Shift+R to force refresh)')
        except Exception:
            logging.exception('Icon refresh support init failed')

    def _check_icon_update(self):
        try:
            png = getattr(self, '_png_icon_path', None)
            if not png or not png.exists():
                return
            mtime = png.stat().st_mtime
            if mtime > getattr(self, '_icon_last_mtime', 0):
                logging.info('Detected icon PNG change; refreshing')
                self._apply_app_icon()
        except Exception:
            logging.exception('Icon update check failed')

    def _manual_icon_refresh(self):  # triggered by shortcut
        try:
            logging.info('Manual icon refresh invoked')
            self._apply_app_icon()
        except Exception:
            logging.exception('Manual icon refresh failed')


def main():
    directory = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home() / "Downloads"
    max_items = int(sys.argv[2]) if len(sys.argv) > 2 else 15
    logging.info(f'Launching application with directory={directory} max_items={max_items}')
    app = TaskbarResidentApp(sys.argv, directory, max_items=max_items)
    rc = app.exec()
    logging.info(f'Application exit code {rc}')
    sys.exit(rc)


if __name__ == "__main__":
    main()
