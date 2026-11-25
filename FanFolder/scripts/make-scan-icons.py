#!/usr/bin/env python3
"""Generate the animated "scanning" variants of the FanFolder icons.

While the app is scanning a folder (menu prewarm) or waiting for a full-text
search walk, the taskbar and tray icons show a bold blue progress ring around a
shrunk copy of the normal artwork.  One .ico per frame, each with the usual
Windows size ladder, so the C++ side can swap frames with WM_SETICON and
Shell_NotifyIcon.

    python3 scripts/make-scan-icons.py

Outputs into resources/:
    app-scan-0.ico .. app-scan-3.ico        (from app.ico, folded plate)
    app-open-scan-0.ico .. -3.ico           (from app-open.ico, transparent)

and a contact sheet at resources/../preview-scan-icons.png.
"""
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
RES = HERE.parent / "resources"

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
FRAMES = 4
ARC_DEGREES = 270.0        # 270 degrees of ring, 90 degrees of gap
BASE_SCALE = 0.62          # artwork shrunk so the ring has room around it
RING_COLOR = (59, 130, 246, 255)   # #3B82F6, matches the drop-hover hint
SUPERSAMPLE = 8


def ring_layer(size: int, angle: float) -> Image.Image:
    ss = size * SUPERSAMPLE
    layer = Image.new("RGBA", (ss, ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    pad = ss * 0.02
    box = [pad, pad, ss - 1 - pad, ss - 1 - pad]
    # A thicker ring at 16/20 px keeps the arc readable in the tray.
    width = max(1, int(round(ss * (0.14 if size <= 24 else 0.10))))
    d.arc(box, start=angle, end=angle + ARC_DEGREES, fill=RING_COLOR, width=width)
    return layer.resize((size, size), Image.LANCZOS)


def compose(base: Image.Image, size: int, angle: float) -> Image.Image:
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    art_src = base.ico.getimage((256, 256)).convert("RGBA")
    inner = max(6, int(round(size * BASE_SCALE)))
    art = art_src.resize((inner, inner), Image.LANCZOS)
    off = (size - inner) // 2
    canvas.alpha_composite(art, (off, off))
    canvas.alpha_composite(ring_layer(size, angle))
    return canvas


def build(base_path: Path, out_prefix: str) -> None:
    base = Image.open(base_path)
    for frame in range(FRAMES):
        angle = frame * (360.0 / FRAMES)
        frames = [compose(base, size, angle) for size in SIZES]
        out = RES / f"{out_prefix}-{frame}.ico"
        frames[-1].save(out, format="ICO", sizes=[(s, s) for s in SIZES])
        print("wrote", out.relative_to(RES.parent))


OVERLAY_SIZES = [16, 20, 24, 32, 48]


def overlay_frame(size: int, angle: float) -> Image.Image:
    """Standalone ring badge for ITaskbarList3::SetOverlayIcon.

    The overlay sits on top of the taskbar button, so it needs a faint dark
    halo to stay readable on a light taskbar as well as a dark one.
    """
    ss = size * SUPERSAMPLE
    layer = Image.new("RGBA", (ss, ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    pad = ss * 0.10
    box = [pad, pad, ss - 1 - pad, ss - 1 - pad]
    w = max(1, int(round(ss * 0.19)))
    d.ellipse(box, outline=(0, 0, 0, 110), width=w + max(1, ss // 40))
    d.arc(box, start=angle, end=angle + ARC_DEGREES, fill=RING_COLOR, width=w)
    return layer.resize((size, size), Image.LANCZOS)


def build_overlays() -> None:
    for frame in range(FRAMES):
        angle = frame * (360.0 / FRAMES)
        frames = [overlay_frame(size, angle) for size in OVERLAY_SIZES]
        out = RES / f"scan-overlay-{frame}.ico"
        frames[-1].save(out, format="ICO",
                        sizes=[(s, s) for s in OVERLAY_SIZES])
        print("wrote", out.relative_to(RES.parent))


def contact_sheet() -> None:
    cols = [16, 24, 32, 48]
    cell = 56
    sheet = Image.new("RGBA", (cell * len(cols), cell * FRAMES), (32, 32, 36, 255))
    for frame in range(FRAMES):
        ico = Image.open(RES / f"app-open-scan-{frame}.ico")
        for i, size in enumerate(cols):
            img = ico.ico.getimage((size, size)).convert("RGBA")
            x = i * cell + (cell - size) // 2
            y = frame * cell + (cell - size) // 2
            sheet.alpha_composite(img, (x, y))
    out = HERE.parent / "preview-scan-icons.png"
    sheet.save(out)
    print("wrote", out.name)


if __name__ == "__main__":
    build(RES / "app.ico", "app-scan")
    build(RES / "app-open.ico", "app-open-scan")
    build_overlays()
    contact_sheet()
