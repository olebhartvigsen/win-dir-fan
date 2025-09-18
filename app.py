import sys
import os
from pathlib import Path
import platform
import logging

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

        # tiny invisible main window that will create a taskbar entry
        self.main = QtWidgets.QMainWindow()
        self.main.setWindowTitle("Windows Dir Fan")
        self.main.setFixedSize(1, 1)
        self.main.setWindowOpacity(0.0)
        try:
            self.main.setWindowFlags(QtCore.Qt.Window)  # type: ignore[attr-defined]
        except Exception:
            # fallback: leave default flags
            pass
        self.main.move(-10000, -10000)
        self.main.show()
        logging.debug('Main window created and moved off-screen')

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
            if self._startup_time and now - self._startup_time < 800:
                logging.debug('Activation ignored due to startup grace period')
                return True
            if now - self._last_activation_time < 250:
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

            if self.fan and self.fan.isVisible() and self._last_show_from_taskbar:
                logging.debug('Hiding existing fan (toggle)')
                self.fan.hide()
                self._last_show_from_taskbar = False
            else:
                if not self.fan:
                    logging.debug('Creating FanWindow instance')
                    self.fan = FanWindow(self.directory, max_items=self.max_items)
                logging.debug('Refreshing fan contents')
                self.fan.refresh()
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


def main():
    directory = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home()
    max_items = int(sys.argv[2]) if len(sys.argv) > 2 else 15
    logging.info(f'Launching application with directory={directory} max_items={max_items}')
    app = TaskbarResidentApp(sys.argv, directory, max_items=max_items)
    rc = app.exec()
    logging.info(f'Application exit code {rc}')
    sys.exit(rc)


if __name__ == "__main__":
    main()
