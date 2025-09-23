from __future__ import annotations
"""Clean consolidated fan widget implementation (after full replacement)."""

import os, logging, hashlib, tempfile, time, threading, math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from PySide6 import QtCore, QtGui, QtWidgets
from PySide6.QtCore import Qt

# (Removed ThumbnailWorker during cleanup; synchronous extraction is sufficient.)

logger = logging.getLogger("windows-dir-fan.fan_widget")

IMAGE_EXTS = {'.png', '.jpg', '.jpeg', '.gif', '.bmp', '.webp', '.tif', '.tiff'}
DISK_CACHE_DIR = Path(tempfile.gettempdir()) / "win_dir_fan_cache"
DISK_CACHE_DIR.mkdir(exist_ok=True)
MAX_FACTORY_SIZE = 512
DEFAULT_MAX_ITEMS = 10
INITIAL_IGNORE_MS = 350
STAY_OPEN_MS = 450

class _PollingMouseDetector(QtCore.QThread):
    clicked = QtCore.Signal(int, int)
    def __init__(self, parent: QtCore.QObject, interval: float = 0.12):
        super().__init__(parent); self._run=False; self._interval=interval; self._last=False
    def run(self):  # pragma: no cover
        try: from ctypes import windll
        except Exception: windll=None
        self._run=True
        while self._run:
            try:
                if windll:
                    down=bool(windll.user32.GetAsyncKeyState(0x01)&0x8000)
                    if down and not self._last:
                        pos=QtGui.QCursor.pos(); self.clicked.emit(pos.x(), pos.y())
                    self._last=down
                time.sleep(self._interval)
            except Exception: time.sleep(self._interval)
    def stop(self):  # pragma: no cover
        self._run=False
        try: self.wait(300)
        except Exception: pass

if os.name=='nt':
    import ctypes; from ctypes import wintypes
    user32=ctypes.windll.user32; kernel32=ctypes.windll.kernel32
    WH_MOUSE_LL=14; WM_LBUTTONDOWN=0x0201; WM_RBUTTONDOWN=0x0204; WM_MBUTTONDOWN=0x0207
    class POINT(ctypes.Structure): _fields_=[('x',ctypes.c_long),('y',ctypes.c_long)]
    class MSLLHOOKSTRUCT(ctypes.Structure): _fields_=[('pt',POINT),('mouseData',wintypes.DWORD),('flags',wintypes.DWORD),('time',wintypes.DWORD),('dwExtraInfo',ctypes.POINTER(ctypes.c_ulong))]
    LowLevelMouseProc=ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM)
    class _WindowsMouseHook:  # pragma: no cover
        def __init__(self, target:'FanWindow'):
            self.target=target; self.hook=None; self._cb=None; self._run=False
        def start(self):
            if self._run: return
            self._run=True; threading.Thread(target=self._loop, daemon=True).start()
        def stop(self):
            self._run=False
            try:
                if self.hook: user32.UnhookWindowsHookEx(self.hook); self.hook=None
            except Exception: pass
        def _loop(self):
            def cb(nCode,wParam,lParam):
                try:
                    if nCode>=0 and wParam in (WM_LBUTTONDOWN,WM_RBUTTONDOWN,WM_MBUTTONDOWN):
                        ms=ctypes.cast(lParam, ctypes.POINTER(MSLLHOOKSTRUCT)).contents
                        self.target.globalMouse.emit(ms.pt.x, ms.pt.y, int(wParam))
                except Exception: pass
                return user32.CallNextHookEx(self.hook,nCode,wParam,lParam)
            self._cb=LowLevelMouseProc(cb)
            self.hook=user32.SetWindowsHookExW(WH_MOUSE_LL,self._cb,kernel32.GetModuleHandleW(None),0)
            msg=wintypes.MSG()
            while self._run:
                b=user32.GetMessageW(ctypes.byref(msg),0,0,0)
                if b==0: break
                user32.TranslateMessage(ctypes.byref(msg)); user32.DispatchMessageW(ctypes.byref(msg))
            if self.hook: user32.UnhookWindowsHookEx(self.hook); self.hook=None
else:  # pragma: no cover
    _WindowsMouseHook=None  # type: ignore

try:  # pragma: no cover
    import comtypes  # type: ignore
    from comtypes import GUID  # type: ignore
except Exception:  # pragma: no cover
    comtypes=None  # type: ignore

if os.name=='nt':
    from ctypes import windll, POINTER, byref, sizeof, c_void_p, Structure, c_int32, c_uint32, create_string_buffer
    from ctypes.wintypes import HBITMAP, DWORD
    class BITMAPINFOHEADER(Structure):  # pragma: no cover
        _fields_=[
            ("biSize", c_uint32),("biWidth", c_int32),("biHeight", c_int32),("biPlanes", c_uint32),("biBitCount", c_uint32),
            ("biCompression", c_uint32),("biSizeImage", c_uint32),("biXPelsPerMeter", c_int32),("biYPelsPerMeter", c_int32),
            ("biClrUsed", c_uint32),("biClrImportant", c_uint32),
        ]
else:  # pragma: no cover
    BITMAPINFOHEADER=object  # type: ignore

def _disk_key(path:Path)->Path:
    h=hashlib.sha1(str(path).encode('utf-8','replace')).hexdigest(); return DISK_CACHE_DIR/f"{h}.png"
def _disk_load(p:Path)->Optional[QtGui.QPixmap]:
    try:
        if not p.exists(): return None
        pm=QtGui.QPixmap(str(p)); return pm if pm and not pm.isNull() else None
    except Exception: return None
def _disk_save(p:Path, pm:QtGui.QPixmap):
    try:
        if pm and not pm.isNull(): pm.save(str(p),'PNG')
    except Exception: pass

class FanItem(QtWidgets.QGraphicsPixmapItem):
    def __init__(self, pm:QtGui.QPixmap, path:Path):
        super().__init__(pm if pm and not pm.isNull() else QtGui.QPixmap()); self.path=path; self.label=None  # type: ignore[attr-defined]
        # Enable hover tracking
        self.setAcceptHoverEvents(True)
        self._hover_anim_scale = None  # type: ignore[assignment]
        self._hover_glow = None  # type: ignore[assignment]
    def mousePressEvent(self,e:QtWidgets.QGraphicsSceneMouseEvent):  # type: ignore[name-defined]
        try:
            if self.path.is_file(): os.startfile(str(self.path))  # type: ignore[attr-defined]
            else: QtGui.QDesktopServices.openUrl(QtCore.QUrl.fromLocalFile(str(self.path)))
        except Exception: logger.exception('open failed')
        try:
            sc=self.scene(); views=sc.views() if sc else []
            if views and views[0].window(): views[0].window().hide()
        except Exception: pass
        e.accept()
        super().mousePressEvent(e)
    # --- Hover highlight ---
    def _ensure_glow(self):
        if self._hover_glow is None:
            glow = QtWidgets.QGraphicsDropShadowEffect()
            glow.setBlurRadius(28)
            glow.setColor(QtGui.QColor(255,255,255,160))
            glow.setOffset(0,0)
            self._hover_glow = glow
    def hoverEnterEvent(self, event):  # type: ignore[override]
        try:
            self._ensure_glow()
            if self._hover_glow:
                try: self.setGraphicsEffect(self._hover_glow)
                except Exception: pass
            # Start scale up animation
            start_scale = 1.0
            end_scale = 1.08
            self.setTransformOriginPoint(self.boundingRect().center())
            anim = QtCore.QVariantAnimation()
            anim.setStartValue(start_scale); anim.setEndValue(end_scale)
            anim.setDuration(130)
            anim.setEasingCurve(QtCore.QEasingCurve.Type.OutCubic)
            anim.valueChanged.connect(lambda v, tgt=self: tgt.setTransform(QtGui.QTransform().scale(float(v), float(v))))  # type: ignore
            anim.finished.connect(lambda a=anim: None)
            anim.start()
            self._hover_anim_scale = anim
            # Highlight associated label if present
            lbl=getattr(self,'label',None)
            if lbl and hasattr(lbl,'set_highlighted'):
                try: lbl.set_highlighted(True)
                except Exception: pass
        except Exception:
            pass
        super().hoverEnterEvent(event)
    def hoverLeaveEvent(self, event):  # type: ignore[override]
        try:
            # Remove glow
            if self._hover_glow:
                try:
                    # type: ignore[arg-type] - explicitly clearing effect
                    self.setGraphicsEffect(None)  # type: ignore
                except Exception:
                    pass
            # Animate back to normal scale
            current_transform = self.transform()
            # approximate current scale from m11()
            current_scale = current_transform.m11() if current_transform is not None else 1.08
            anim = QtCore.QVariantAnimation(); anim.setStartValue(current_scale); anim.setEndValue(1.0)
            anim.setDuration(120); anim.setEasingCurve(QtCore.QEasingCurve.Type.OutCubic)
            anim.valueChanged.connect(lambda v, tgt=self: tgt.setTransform(QtGui.QTransform().scale(float(v), float(v))))  # type: ignore
            anim.start(); self._hover_anim_scale = anim
            lbl=getattr(self,'label',None)
            if lbl and hasattr(lbl,'set_highlighted'):
                try: lbl.set_highlighted(False)
                except Exception: pass
        except Exception:
            pass
        super().hoverLeaveEvent(event)

class _StrokeTextItem(QtWidgets.QGraphicsItem):
    """Stroked text label (no background box) with slightly expanded bounding rect
    to avoid clipping from stroke width and highlight scaling."""
    def __init__(self, text: str, base_font: QtGui.QFont):
        super().__init__()
        self._text = text
        self._font = QtGui.QFont(base_font)
        self._font.setBold(True)
        self._font.setWeight(QtGui.QFont.Weight.Bold)
        self._fill = QtGui.QColor(25,25,25)
        self._stroke = QtGui.QColor(255,255,255)
        self._stroke_w = 2.0
        # Removed background box: retain padding vars only for potential future metrics
        self._pad_h = 0.0
        self._pad_v = 0.0
        self._bg_base = None  # type: ignore[assignment]
        self._bg_highlight = None  # type: ignore[assignment]
        self._highlight = False
        self._radius_factor = 0.25
        self._path, self._text_rect, self._outer_rect = self._build_path_and_rects()
        self.setCacheMode(QtWidgets.QGraphicsItem.CacheMode.DeviceCoordinateCache)

    def _build_path_and_rects(self):
        fm = QtGui.QFontMetrics(self._font)
        # Text path baseline at y = ascent to mimic normal drawText(0, ascent)
        path = QtGui.QPainterPath(); path.addText(0, fm.ascent(), self._font, self._text)
        text_rect = path.boundingRect()
        stroke_exp = (self._stroke_w * 0.9)
        text_ext = text_rect.adjusted(-stroke_exp, -stroke_exp, stroke_exp, stroke_exp)
        # Slightly larger outer rect for highlight scale and AA safety.
        # Provide a generous extra right margin to avoid clipping narrow trailing glyphs (e.g. 'l', 't', ')')
        # during stroke + highlight scale. Store the extra so layout can compensate when horizontally positioning.
        extra_right = 12  # was 4; increased to fully prevent right-edge cropping
        outer = text_ext.adjusted(-3, -3, extra_right, 3)
        # Expose for layout logic (used to avoid over-shifting label left when computing x)
        try:
            self._extra_right = extra_right  # type: ignore[attr-defined]
        except Exception:
            pass
        return path, text_rect, outer

    def boundingRect(self):  # type: ignore[override]
        return self._outer_rect

    def paint(self, p: QtGui.QPainter, option, widget=None):  # type: ignore[override]
        p.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
        p.setRenderHint(QtGui.QPainter.RenderHint.TextAntialiasing, True)
        # Translate so that outer rect top-left is (0,0)
        p.translate(-self._outer_rect.left(), -self._outer_rect.top())
        # Draw stroked then filled text only (background removed)
        stroke_col = self._stroke
        fill_col = self._fill
        if self._highlight:
            fill_col = QtGui.QColor(min(255, fill_col.red()+40), min(255, fill_col.green()+40), min(255, fill_col.blue()+40))
        pen = QtGui.QPen(stroke_col); pen.setWidthF(self._stroke_w + (0.8 if self._highlight else 0.0))
        pen.setJoinStyle(QtCore.Qt.PenJoinStyle.RoundJoin); pen.setCapStyle(QtCore.Qt.PenCapStyle.RoundCap)
        p.setPen(pen); p.setBrush(QtCore.Qt.BrushStyle.NoBrush); p.drawPath(self._path.translated(-self._outer_rect.left(), -self._outer_rect.top()))
        p.setPen(QtCore.Qt.PenStyle.NoPen); p.setBrush(fill_col); p.drawPath(self._path.translated(-self._outer_rect.left(), -self._outer_rect.top()))

    def set_highlighted(self, on: bool):
        if self._highlight == on:
            return
        self._highlight = on
        try:
            scale = 1.06 if on else 1.0
            self.setTransform(QtGui.QTransform().scale(scale, scale))
        except Exception:
            pass
        try: self.setCacheMode(QtWidgets.QGraphicsItem.CacheMode.NoCache)
        except Exception: pass
        self.update()
        try: self.setCacheMode(QtWidgets.QGraphicsItem.CacheMode.DeviceCoordinateCache)
        except Exception: pass

class FanWindow(QtWidgets.QWidget):
    globalMouse=QtCore.Signal(int,int,int)
    def __init__(self, directory:Path, max_items:int=DEFAULT_MAX_ITEMS):
        super().__init__()
        flags = (
            QtCore.Qt.WindowType.Tool
            | QtCore.Qt.WindowType.FramelessWindowHint
            | QtCore.Qt.WindowType.WindowStaysOnTopHint
            | QtCore.Qt.WindowType.NoDropShadowWindowHint
        )
        self.setWindowFlags(flags)
        try:
            self.setAttribute(QtCore.Qt.WidgetAttribute.WA_TranslucentBackground, True)
        except Exception: pass
        self.setFocusPolicy(QtCore.Qt.FocusPolicy.StrongFocus)
        self.directory=Path(directory); self.max_items=max(1,int(max_items)); self.thumb_size=96
        self.scene=QtWidgets.QGraphicsScene(self)
        self.view=QtWidgets.QGraphicsView(self.scene,self)
        self.view.setStyleSheet("QGraphicsView{background:transparent;border:0;padding:0;}")
        self.view.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.view.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.view.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        lay=QtWidgets.QVBoxLayout(self)
        lay.setContentsMargins(0,0,0,0)
        lay.addWidget(self.view)
        # runtime attributes (avoid inline type annotations for simpler runtime compatibility)
        self.items=[]  # type: List[FanItem]
        self._anchor_x=None  # type: Optional[int]
        self._mem_cache={}  # type: Dict[str,Tuple[float,QtGui.QPixmap]]
        self._poll_detector=None  # type: Optional[_PollingMouseDetector]
        self._mouse_hook=None  # type: Optional['_WindowsMouseHook']
        self._hide_timer=None  # type: Optional[QtCore.QTimer]
        self._initial_ignore_ms=INITIAL_IGNORE_MS
        self._stay_open_ms=STAY_OPEN_MS
        self._ignore_mouse_until=0
        self._ignore_focus_until=0
        self._stay_open_until=0
        self._filter_installed=False
    # ThumbnailWorker removed: previously handled async image thumbnails.
        # feature flags / options
        self.show_filenames = True  # show filename labels to the left of icons
        self.include_home_arrow = True  # always show a top arrow icon to open the base directory
    # Animation system fully removed – fan appears instantly.
        # Label font sizing parameters (allows slightly larger default text)
        self.label_font_min = 9.5
        self.label_font_max = 16.0
        self.label_font_divisor = 8.0  # lower divisor => larger font for same thumb size
        # Spacing between label text and icon (was fixed 8)
        self.label_icon_gap = 14
        # Vertical spacing tuning (factor applied to base thumb-derived spacing + additive extra)
        self.vertical_spacing_factor = 1.10  # >1.0 increases space between rows
        self.vertical_spacing_extra = 4       # constant extra pixels between icons
        # Curvature tuning: allows smoother (more rounded) horizontal fan curve
        # base_curve_factor: scales overall horizontal spread
        # per_item_curve_factor: incremental growth per item count (kept from legacy 0.04 baseline)
        # curve_style: 'quadratic' (legacy) or 'sine' (rounded) or 'exp'
        self.base_curve_factor = 0.80
        self.per_item_curve_factor = 0.045  # slightly higher than previous 0.04 to increase curvature
        self.curve_style = 'sine'  # default to a more rounded appearance
        # Taskbar blank thumbnail support (Windows only)
        self._blank_thumb_hbitmap = None  # type: ignore[assignment]
        self._dwm_inited = False
        if os.name == 'nt':
            try:
                # Force native window creation so we have a HWND
                _ = int(self.winId())
                self._init_blank_taskbar_thumbnail()
            except Exception:
                logger.exception('init blank taskbar thumbnail failed')
        # In future could be exposed via a config or hotkey.
        logger.debug(f'FanWindow created dir={self.directory} max_items={self.max_items}')

    # --- Windows taskbar thumbnail suppression (blank iconic bitmap) ---
    def _init_blank_taskbar_thumbnail(self):  # pragma: no cover (Windows specific UI behavior)
        if self._dwm_inited:
            return
        try:
            import ctypes
            from ctypes import wintypes as _w
            dwmapi = ctypes.windll.dwmapi
            user32_local = ctypes.windll.user32
            gdi32_local = ctypes.windll.gdi32
            # Constants
            self._WM_DWMSENDICONICTHUMBNAIL = 0x0323
            self._WM_DWMSENDICONICLIVEPREVIEWBITMAP = 0x0326
            self._DWMWA_FORCE_ICONIC_REPRESENTATION = 7
            self._DWMWA_HAS_ICONIC_BITMAP = 10
            self._DWMWA_DISALLOW_PEEK = 11
            self._DWMWA_EXCLUDED_FROM_PEEK = 12
            hwnd = _w.HWND(int(self.winId()))
            def _set_attr(attr, value=1):
                val = _w.DWORD(int(value))
                dwmapi.DwmSetWindowAttribute(hwnd, attr, ctypes.byref(val), ctypes.sizeof(val))
            for a in (
                self._DWMWA_FORCE_ICONIC_REPRESENTATION,
                self._DWMWA_HAS_ICONIC_BITMAP,
                self._DWMWA_DISALLOW_PEEK,
                self._DWMWA_EXCLUDED_FROM_PEEK,
            ):
                _set_attr(a, 1)
            # Create 1x1 fully transparent ARGB bitmap
            bi = ctypes.create_string_buffer(40 + 16)  # BITMAPINFO + masks
            ctypes.memset(bi, 0, len(bi))
            # BITMAPINFOHEADER fields (little endian layout assumptions)
            ctypes.cast(bi, ctypes.POINTER(ctypes.c_uint32))[0] = 40  # biSize
            ctypes.cast(bi, ctypes.POINTER(ctypes.c_int))[1] = 1      # biWidth
            ctypes.cast(bi, ctypes.POINTER(ctypes.c_int))[2] = 1      # biHeight
            ctypes.cast(bi, ctypes.POINTER(ctypes.c_ushort))[6] = 1   # biPlanes
            ctypes.cast(bi, ctypes.POINTER(ctypes.c_ushort))[7] = 32  # biBitCount
            ctypes.cast(bi, ctypes.POINTER(ctypes.c_uint32))[5] = 3   # BI_BITFIELDS
            mask_offset = 40 // 4
            arr = ctypes.cast(bi, ctypes.POINTER(ctypes.c_uint32))
            arr[mask_offset + 0] = 0x00FF0000  # R
            arr[mask_offset + 1] = 0x0000FF00  # G
            arr[mask_offset + 2] = 0x000000FF  # B
            arr[mask_offset + 3] = 0xFF000000  # A
            bits_ptr = ctypes.c_void_p()
            hdc = user32_local.GetDC(0)
            hbitmap = gdi32_local.CreateDIBSection(hdc, bi, 0, ctypes.byref(bits_ptr), None, 0)
            user32_local.ReleaseDC(0, hdc)
            self._blank_thumb_hbitmap = hbitmap
            # Store function refs for later usage
            self._DwmSetIconicThumbnail = dwmapi.DwmSetIconicThumbnail
            self._DwmSetIconicLivePreviewBitmap = dwmapi.DwmSetIconicLivePreviewBitmap
            self._dwm_inited = True
        except Exception:
            logger.exception('blank taskbar thumbnail setup failed')

    def nativeEvent(self, eventType, message):  # type: ignore[override]
        """Intercept Windows DWM iconic thumbnail requests to supply a transparent bitmap.

        Keep signature minimal (Qt may pass a QByteArray for eventType); we only check for the
        string literal 'windows_generic_MSG'. If types differ we silently ignore.
        """
        try:
            if os.name == 'nt' and self._dwm_inited:
                et = eventType if isinstance(eventType, str) else (eventType.data().decode() if hasattr(eventType, 'data') else None)  # type: ignore
                if et == 'windows_generic_MSG':
                    import ctypes
                    from ctypes import wintypes as _w
                    msg = ctypes.cast(message.__int__(), ctypes.POINTER(_w.MSG)).contents
                    if msg.message == self._WM_DWMSENDICONICTHUMBNAIL and self._blank_thumb_hbitmap:
                        try:
                            self._DwmSetIconicThumbnail(_w.HWND(int(self.winId())), self._blank_thumb_hbitmap, 0)
                        except Exception:
                            pass
                        return True, 0
                    elif msg.message == self._WM_DWMSENDICONICLIVEPREVIEWBITMAP and self._blank_thumb_hbitmap:
                        try:
                            self._DwmSetIconicLivePreviewBitmap(_w.HWND(int(self.winId())), self._blank_thumb_hbitmap, None, 0)
                        except Exception:
                            pass
                        return True, 0
        except Exception:
            pass
        # Call base implementation only if eventType is a QByteArray-like; otherwise return default.
        try:
            from PySide6.QtCore import QByteArray  # type: ignore
            if isinstance(eventType, (QByteArray, bytes, bytearray, memoryview)):
                return QtWidgets.QWidget.nativeEvent(self, eventType, message)  # type: ignore
        except Exception:
            pass
        return False, 0

    def __del__(self):  # pragma: no cover
        # Cleanup bitmap resource if created
        try:
            if os.name == 'nt' and self._blank_thumb_hbitmap:
                import ctypes
                ctypes.windll.gdi32.DeleteObject(self._blank_thumb_hbitmap)
        except Exception:
            pass

    def set_anchor_x(self,x:int):
        self._anchor_x=x
        self._recenter_to_anchor()
    def refresh(self):
        logger.debug('FanWindow.refresh begin')
        for it in list(self.items):
            try:
                # remove label first (if present) then icon item
                if hasattr(it,'label') and it.label:
                    try: self.scene.removeItem(it.label)
                    except Exception: pass
                self.scene.removeItem(it)
            except Exception: pass
        self.items.clear(); files=self._recent_files(); self._update_thumb_size(len(files))
        # If arrow enabled, reduce file list to keep total within max_items
        if self.include_home_arrow:
            if len(files) > self.max_items - 1:
                files = files[: self.max_items - 1]
        else:
            if len(files) > self.max_items:
                files = files[: self.max_items]

        # Add arrow item first (topmost)
        if self.include_home_arrow:
            try:
                arrow_pm = self._arrow_pixmap()
                arrow_item = FanItem(arrow_pm, self.directory)
                arrow_item.is_arrow = True  # type: ignore[attr-defined]
                self.scene.addItem(arrow_item)
                self.items.append(arrow_item)
            except Exception:
                logger.exception('failed creating arrow item')

        for idx, p in enumerate(files):
            pm=self._load_icon_for(p); item=FanItem(pm,p); self.scene.addItem(item); self.items.append(item)
            # add label (filename) to the left if enabled
            if self.show_filenames:
                try:
                    # Skip label for the synthetic arrow item
                    if getattr(item, 'is_arrow', False):
                        item.label=None  # type: ignore[attr-defined]
                    else:
                        font=QtGui.QFont()
                        computed_size = self.thumb_size / self.label_font_divisor if self.label_font_divisor else 12.0
                        font_size = max(self.label_font_min, min(self.label_font_max, computed_size))
                        font.setPointSizeF(font_size)
                        metrics=QtGui.QFontMetrics(font)
                        max_display_width=280  # px cap to avoid ultra-wide window
                        name=p.name
                        elided=metrics.elidedText(name, QtCore.Qt.TextElideMode.ElideMiddle, max_display_width)
                        label_item=_StrokeTextItem(elided, font)
                        self.scene.addItem(label_item)
                        item.label=label_item  # type: ignore[attr-defined]
                except Exception:
                    item.label=None  # type: ignore[attr-defined]
            else:
                item.label=None  # type: ignore[attr-defined]
            # Stronger initial hide: set hidden animation state immediately per item addition
            # (Animation prep removed)
        self.relayout()
    # Refresh complete.

    # (Animation helpers removed)
    def _recent_files(self)->List[Path]:
        try:
            entries=[]
            for child in self.directory.iterdir():
                if child.name.startswith('.'): continue
                try:
                    st=child.stat();
                    if child.is_file() or child.is_dir(): entries.append((st.st_mtime, child))
                except Exception: pass
            entries.sort(reverse=True)
            return [p for _,p in entries[:self.max_items]]
        except Exception:
            logger.exception('list directory failed'); return []
    def _arrow_pixmap(self) -> QtGui.QPixmap:
        """Build an arrow pixmap (upward pointing) sized to current thumb_size."""
        size = max(32, int(self.thumb_size))
        pm = QtGui.QPixmap(size, size)
        pm.fill(QtCore.Qt.GlobalColor.transparent)
        try:
            painter = QtGui.QPainter(pm)
            painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
            # Background circle for contrast
            bg_color = QtGui.QColor(30,30,30,180)
            painter.setPen(QtCore.Qt.PenStyle.NoPen)
            painter.setBrush(bg_color)
            margin = size*0.05
            painter.drawEllipse(QtCore.QRectF(margin, margin, size-2*margin, size-2*margin))
            # Arrow
            stroke = QtGui.QPen(QtGui.QColor(255,255,255,240))
            stroke.setWidthF(max(2.0, size*0.08))
            stroke.setCapStyle(QtCore.Qt.PenCapStyle.RoundCap)
            stroke.setJoinStyle(QtCore.Qt.PenJoinStyle.RoundJoin)
            painter.setPen(stroke)
            painter.setBrush(QtCore.Qt.BrushStyle.NoBrush)
            shaft_top = size*0.28
            shaft_bottom = size*0.70
            center_x = size/2
            # Shaft
            painter.drawLine(QtCore.QPointF(center_x, shaft_bottom), QtCore.QPointF(center_x, shaft_top))
            # Head (triangle)
            head_w = size*0.28
            head_h = size*0.26
            path = QtGui.QPainterPath()
            path.moveTo(center_x, shaft_top - head_h)  # tip
            path.lineTo(center_x - head_w/2, shaft_top)
            path.lineTo(center_x + head_w/2, shaft_top)
            path.closeSubpath()
            painter.fillPath(path, QtGui.QColor(255,255,255,240))
        except Exception:
            logger.exception('arrow pixmap draw failed')
        finally:
            try: painter.end()
            except Exception: pass
        return pm
    def _update_thumb_size(self,count:int):
        try:
            screen=QtWidgets.QApplication.primaryScreen(); avail=screen.availableGeometry() if screen else QtCore.QRect(0,0,1280,720)
            target=int(avail.height()*0.5)
            size=max(48,min(256,int(target/max(1,count+0.15)))) if count>0 else 96
            if size!=self.thumb_size: logger.debug(f'thumb {self.thumb_size}->{size} count={count}')
            self.thumb_size=size
        except Exception: pass
    def _load_icon_for(self,path:Path)->QtGui.QPixmap:
        try:
            mtime=path.stat().st_mtime; key=str(path); c=self._mem_cache.get(key)
            if c and c[0]==mtime and c[1] and not c[1].isNull(): return self._scale_square(c[1])
        except Exception: pass
        dk=_disk_key(path); pm=_disk_load(dk)
        if pm is not None:
            try: self._mem_cache[str(path)]=(path.stat().st_mtime, pm)
            except Exception: pass
            return self._scale_square(pm)
        pm=self._extract_high_res(path)
        if pm is None or pm.isNull(): pm=QtGui.QPixmap(self.thumb_size,self.thumb_size); pm.fill(QtGui.QColor(80,80,80,220))
        else:
            _disk_save(dk, pm)
            try: self._mem_cache[str(path)]=(path.stat().st_mtime, pm)
            except Exception: pass
        return self._scale_square(pm)
    def _scale_square(self,pm:QtGui.QPixmap)->QtGui.QPixmap:
        try:
            if pm.isNull(): return pm
            w,h=pm.width(), pm.height(); side=max(w,h)
            if w!=h:
                sq=QtGui.QPixmap(side,side); sq.fill(QtCore.Qt.GlobalColor.transparent); painter=QtGui.QPainter(sq)
                try: painter.drawPixmap((side-w)//2,(side-h)//2,pm)
                finally: painter.end()
                pm=sq
            return pm.scaled(
                self.thumb_size,
                self.thumb_size,
                QtCore.Qt.AspectRatioMode.KeepAspectRatio,
                QtCore.Qt.TransformationMode.SmoothTransformation,
            )
        except Exception: return pm
    def _extract_high_res(self,path:Path)->Optional[QtGui.QPixmap]:
        if os.name!='nt':
            return QtGui.QIcon.fromTheme(path.suffix.lstrip('.')).pixmap(self.thumb_size,self.thumb_size)
        if path.suffix.lower() in IMAGE_EXTS:
            pm=QtGui.QPixmap(str(path))
            if not pm.isNull(): return pm
        pm=self._shell_image_factory(path);  # noqa
        if pm: return pm
        if path.suffix.lower()=='.pdf':
            pm=self._pdf_icon();
            if pm: return pm
        pm=self._system_image(path)
        if pm: return pm
        try:
            icon=QtWidgets.QFileIconProvider().icon(QtCore.QFileInfo(str(path)))
            pm=icon.pixmap(MAX_FACTORY_SIZE, MAX_FACTORY_SIZE)
            if pm and not pm.isNull(): return pm
        except Exception: pass
        return None
    def _shell_image_factory(self,path:Path)->Optional[QtGui.QPixmap]:  # pragma: no cover
        if comtypes is None: return None
        try:
            from comtypes.shell import shell, shellcon  # type: ignore
            flags_icon=getattr(shellcon,'SIIGBF_ICONONLY',0x4); flags_thumb=getattr(shellcon,'SIIGBF_THUMBNAILONLY',0x1)
            for flags in (flags_icon, flags_thumb):
                try:
                    shitem=shell.SHCreateItemFromParsingName(str(path), None, GUID('{43826d1e-e718-42ee-bc55-a1e261c37bfe}'))
                    factory=shitem.QueryInterface(GUID('{bcc18b79-ba16-442f-80c4-8a59c30c463b}'))
                    hbitmap,_=factory.GetImage((MAX_FACTORY_SIZE,MAX_FACTORY_SIZE), flags)
                    pm=self._from_hbitmap(hbitmap)
                    if pm and not pm.isNull(): return pm
                except Exception: continue
        except Exception: logger.exception('ShellItemImageFactory failure')
        return None
    def _pdf_icon(self)->Optional[QtGui.QPixmap]:  # pragma: no cover
        try:
            import winreg  # type: ignore
            with winreg.OpenKey(winreg.HKEY_CLASSES_ROOT, r'AcroExch.Document.DC\\DefaultIcon') as k:
                # Stub types complain about None; use empty string which typically raises, so guard.
                try:
                    val,_=winreg.QueryValueEx(k, None)  # type: ignore[arg-type]
                except Exception:
                    try:
                        val,_=winreg.QueryValueEx(k, "")  # fallback
                    except Exception:
                        val=""
                if val:
                    parts=val.split(','); dll=parts[0].strip().strip('"'); idx=int(parts[1]) if len(parts)>1 else 0
                    pm=self._icon_from_library(dll,idx)
                    if pm: return pm
        except Exception: pass
        return None
    def _icon_from_library(self,dll_path:str,index:int)->Optional[QtGui.QPixmap]:  # pragma: no cover
        try:
            import ctypes
            Extract=ctypes.windll.shell32.ExtractIconExW; large=(ctypes.c_void_p*1)(); small=(ctypes.c_void_p*1)()
            count=Extract(dll_path,index,large,small,1)
            if count>0 and large[0]:
                img=QtGui.QImage.fromHICON(int(large[0]))  # type: ignore[attr-defined]
                if img and not img.isNull(): return QtGui.QPixmap.fromImage(img)
        except Exception: pass
        return None
    def _system_image(self,path:Path)->Optional[QtGui.QPixmap]: return None  # placeholder
    def _from_hbitmap(self,hbitmap)->Optional[QtGui.QPixmap]:  # pragma: no cover
        try:
            if not hbitmap: return None
            hdc=windll.user32.GetDC(0); bmi=BITMAPINFOHEADER(); bmi.biSize=sizeof(BITMAPINFOHEADER)
            windll.gdi32.GetDIBits(hdc,int(hbitmap),0,0,None,byref(bmi),0)
            bitcount=int(bmi.biBitCount); width=int(bmi.biWidth); height_signed=int(bmi.biHeight)
            if width<=0 or height_signed==0 or bitcount not in (24,32): windll.user32.ReleaseDC(0,hdc); windll.gdi32.DeleteObject(int(hbitmap)); return None
            height_abs=abs(height_signed); bpp=bitcount//8; row_bytes=width*bpp; stride=(row_bytes+3)&~3; buf_size=stride*height_abs; pixel_data=create_string_buffer(buf_size); bmi.biCompression=0
            got=windll.gdi32.GetDIBits(hdc,int(hbitmap),0,height_abs,pixel_data,byref(bmi),0); windll.user32.ReleaseDC(0,hdc); windll.gdi32.DeleteObject(int(hbitmap))
            if not got: return None
            import numpy as _np  # type: ignore
            arr=_np.frombuffer(pixel_data.raw,dtype=_np.uint8)
            if stride!=row_bytes: arr=arr.reshape((height_abs,stride))[:, :row_bytes]
            arr=arr.reshape((height_abs,width,bpp))
            if height_signed>0: arr=arr[::-1]
            if bpp==3: arr=_np.concatenate([arr,_np.full((height_abs,width,1),255,dtype=_np.uint8)],axis=2)
            arr=arr.copy(); arr[:,:, [0,2]]=arr[:,:, [2,0]]; alpha=arr[:,:,3];
            if not _np.any(alpha): alpha[:]=255
            qimg=QtGui.QImage(arr.data,width,height_abs,width*4,QtGui.QImage.Format.Format_RGBA8888)  # type: ignore[attr-defined]
            return QtGui.QPixmap.fromImage(qimg.copy()) if qimg and not qimg.isNull() else None
        except Exception: logger.exception('from_hbitmap failed'); return None
    def relayout(self):
        n=len(self.items)
        if n==0: return
        left=4; top=2
        base_spacing = int(self.thumb_size*0.85)
        try:
            v_spacing = int(base_spacing * getattr(self, 'vertical_spacing_factor', 1.0) + getattr(self, 'vertical_spacing_extra', 0))
        except Exception:
            v_spacing = base_spacing
        if v_spacing < 4:
            v_spacing = 4
        # Compute base curve magnitude using configurable factors
        curve = self.thumb_size * (self.base_curve_factor + self.per_item_curve_factor * n)
        label_gap=self.label_icon_gap if self.show_filenames else 0
        # compute max label width (if any)
        max_label_width=0
        if self.show_filenames:
            for it in self.items:
                lbl=getattr(it,'label',None)
                if lbl:
                    try:
                        # Use full outer bounding rect width for spacing now (prevents clipping)
                        max_label_width=max(max_label_width, int(lbl.boundingRect().width()))
                    except Exception: pass
            # safety cap (should match elide width to remain consistent)
            max_label_width=min(max_label_width, 280)
        offs=[]; max_off=0.0
        if n>1:
            for i in range(n):
                # Position index from top (0) to bottom (n-1); we want top to have smallest offset, bottom largest
                t = i / (n-1)  # 0..1 top->bottom
                if self.curve_style == 'sine':
                    # Rounded: half sine wave easing (ease-in accelerating toward bottom)
                    # Use sin(t * pi/2) to map 0->1 with smooth start
                    weight = math.sin(t * math.pi / 2.0)
                elif self.curve_style == 'exp':
                    # Exponential emphasis toward bottom
                    weight = (math.pow(t, 1.8))
                else:  # 'quadratic' legacy
                    weight = t * t  # ease-in quadratic
                off = weight * curve
                offs.append(off)
                if off>max_off: max_off=off
        else:
            offs=[0.0]; max_off=0.0
        req=int(left+max_label_width+label_gap+max_off+self.thumb_size+left)
        if req>self.width():
            try: self.resize(req,self.height())
            except Exception: pass
        width=self.width() or req; base_x=left+max_label_width+label_gap+max_off; last_y=0; last_h=self.thumb_size
        for i,(it,off) in enumerate(zip(self.items,offs)):
            pix=it.pixmap(); h=pix.height(); x=int(base_x-off); y=int(top+i*v_spacing); it.setPos(x,y); it.setRotation(0); last_y=y; last_h=h
            # position label to the left (vertically centered relative to icon)
            if self.show_filenames:
                lbl=getattr(it,'label',None)
                if lbl:
                    try:
                        lb=lbl.boundingRect();
                        # Compensate for artificially added right margin so label doesn't shift too far left.
                        extra_right = getattr(lbl, '_extra_right', 0)
                        effective_width = int(lb.width() - extra_right) if lb.width() > extra_right else int(lb.width())
                        # Add a couple extra px to width to counter any AA/trailing stroke shape
                        label_x=x-label_gap-effective_width-2
                        label_y=y+max(0,(h-lb.height())/2)
                        lbl.setPos(label_x,label_y)
                    except Exception: pass
        # Assign Z-order so that items further from the taskbar (smaller y, higher vertically) appear above lower ones.
        try:
            n=len(self.items)
            for i,it in enumerate(self.items):
                # smaller y => higher z; top item index 0 gets largest z
                z=float(n - i)
                it.setZValue(z)
                lbl=getattr(it,'label',None)
                if lbl:
                    try: lbl.setZValue(z-0.1)  # label slightly below its icon
                    except Exception: pass
        except Exception:
            logger.exception('failed setting z-order')
        total=int(last_y+last_h+1)
        try: self.scene.setSceneRect(0,0,width,total)
        except Exception: pass
        try:
            if self.height()!=total:
                self.resize(width,total)
        except Exception:
            pass
        self._bottom_align_and_anchor()
    def _bottom_align_and_anchor(self):
        if not self.isVisible(): return
        try:
            screen=QtWidgets.QApplication.primaryScreen();
            if not screen: return
            avail=screen.availableGeometry(); new_y=avail.bottom()-self.height()+1; new_x=self.x()
            if self._anchor_x is not None and self.items:
                bottom=self.items[-1]; pix=bottom.pixmap(); center_local=bottom.pos().x()+pix.width()/2; new_x=self.x()+int(self._anchor_x-(self.x()+center_local))
            new_x=max(avail.left(), min(new_x, avail.right()-self.width())); self.move(new_x,new_y)
        except Exception: logger.exception('anchor align failed')
    def _recenter_to_anchor(self):
        if self.isVisible(): self._bottom_align_and_anchor()
    def show(self):
        now=QtCore.QDateTime.currentMSecsSinceEpoch(); self._ignore_mouse_until=now+self._initial_ignore_ms; self._ignore_focus_until=now+self._initial_ignore_ms; self._stay_open_until=now+self._stay_open_ms
        super().show(); self.install_global_filter(); self._start_hooks(); QtCore.QTimer.singleShot(0,self._bottom_align_and_anchor)
    def hide(self):
        # (Animation removed) nothing special to cancel
        self._remove_hooks(); self.uninstall_global_filter(); super().hide()
    def showEvent(self,e:QtGui.QShowEvent):  # type: ignore[override]
        super().showEvent(e)
        self.setFocus(QtCore.Qt.FocusReason.OtherFocusReason)
    def focusOutEvent(self,e:QtGui.QFocusEvent):  # type: ignore[override]
        now=QtCore.QDateTime.currentMSecsSinceEpoch();
        if self._ignore_focus_until and now<self._ignore_focus_until: return super().focusOutEvent(e)
        if self._stay_open_until and now<self._stay_open_until: return super().focusOutEvent(e)
        self.hide(); super().focusOutEvent(e)
    def install_global_filter(self):
        if self._filter_installed: return
        app = QtWidgets.QApplication.instance()
        if app:
            app.installEventFilter(self)
            self._filter_installed=True
    def uninstall_global_filter(self):
        if not self._filter_installed: return
        try:
            app = QtWidgets.QApplication.instance()
            if app:
                app.removeEventFilter(self)
        except Exception: pass
        self._filter_installed=False
    def eventFilter(self,obj,event):  # type: ignore[override]
        try:
            if event.type()==QtCore.QEvent.Type.MouseButtonPress:
                pos=event.globalPosition().toPoint() if hasattr(event,'globalPosition') else QtGui.QCursor.pos(); now=QtCore.QDateTime.currentMSecsSinceEpoch()
                if self._ignore_mouse_until and now<self._ignore_mouse_until: return False
                if self._stay_open_until and now<self._stay_open_until: return False
                if not self.geometry().contains(pos): self.hide()
        except Exception: pass
        return super().eventFilter(obj,event)
    def _start_hooks(self):
        if os.name=='nt' and _WindowsMouseHook and self._mouse_hook is None:
            try: self._mouse_hook=_WindowsMouseHook(self); self._mouse_hook.start(); self.globalMouse.connect(self._on_global_mouse)
            except Exception: pass
        if self._poll_detector is None:
            try: self._poll_detector=_PollingMouseDetector(self); self._poll_detector.clicked.connect(self._on_poll_click); self._poll_detector.start()
            except Exception: pass
    def _remove_hooks(self):
        if self._mouse_hook:
            try: self.globalMouse.disconnect(self._on_global_mouse)
            except Exception: pass
            try: self._mouse_hook.stop()
            except Exception: pass
            self._mouse_hook=None
        if self._poll_detector:
            try: self._poll_detector.stop()
            except Exception: pass
            self._poll_detector=None
    @QtCore.Slot(int,int,int)
    def _on_global_mouse(self,x:int,y:int,w:int):
        try:
            now=QtCore.QDateTime.currentMSecsSinceEpoch()
            if self._ignore_mouse_until and now<self._ignore_mouse_until: return
            if self._stay_open_until and now<self._stay_open_until: return
            if not self.geometry().contains(x,y): self.hide()
        except Exception: pass
    def _on_poll_click(self,x:int,y:int):
        try:
            now=QtCore.QDateTime.currentMSecsSinceEpoch()
            if self._ignore_mouse_until and now<self._ignore_mouse_until: return
            if self._stay_open_until and now<self._stay_open_until: return
            if not self.geometry().contains(x,y): self.hide()
        except Exception: pass
    # Background worker callback removed.
__all__=["FanWindow","FanItem"]
