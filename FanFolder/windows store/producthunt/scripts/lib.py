"""Shared rendering helpers for the FanFolder Product Hunt gallery."""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

# Product Hunt gallery is 1270x760; render at 2x for a crisp retina asset.
W, H = 2540, 1520
S = 2  # scale factor vs the 1270x760 design grid

FB = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
FR = "/System/Library/Fonts/Supplemental/Arial.ttf"

INK = (17, 22, 34)
BLUE = (28, 110, 226)
DIM = (88, 99, 118)
FAINT = (124, 135, 154)
WHITE = INK  # headline colour on the light theme

HERE = os.path.dirname(os.path.abspath(__file__))
# captured app frame, already relabelled from Danish to English
FRAME = os.path.join(HERE, "frames", "app-frame.png")
# rendered slides land one level up, next to the demo GIF
OUT = os.path.dirname(HERE)


def bold(px):
    return ImageFont.truetype(FB, int(px * S))


def reg(px):
    return ImageFont.truetype(FR, int(px * S))


def background(glow_x=None, glow_y=None, glow_r=460, strength=46):
    """Near-white canvas with light structure: a fine dot grid plus a soft
    cool wash. Anywhere a screenshot lands, call clear_zone() afterwards to
    dissolve the structure back to pure white so no box edge can form.
    """
    bg = Image.new("RGB", (W, H), (255, 255, 255))

    # soft cool wash, strongest in the upper left, keeps the page from
    # looking like a blank sheet
    wash = Image.new("RGB", (W, H), (0, 0, 0))
    wd = ImageDraw.Draw(wash)
    cx, cy = int(W * 0.22), int(H * 0.12)
    R = int(W * 0.78)
    for r in range(R, 0, -12):
        v = 30 * (1 - r / R) ** 1.6
        wd.ellipse([cx - r, cy - r, cx + r, cy + r],
                   fill=(int(v * 0.34), int(v * 0.20), 0))
    wash = wash.filter(ImageFilter.GaussianBlur(40 * S))
    bg = ImageChops.subtract(bg, wash)

    # fine dot grid
    dots = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(dots)
    step = 30 * S
    rad = int(1.6 * S)
    for y in range(step, H, step):
        for x in range(step, W, step):
            dd.ellipse([x - rad, y - rad, x + rad, y + rad], fill=(17, 22, 34, 62))
    bg = Image.alpha_composite(bg.convert("RGBA"), dots)

    # hairline rule along the very bottom for a grounded edge
    ImageDraw.Draw(bg).rectangle([0, H - 3 * S, W, H], fill=(17, 22, 34, 16))
    return bg


def clear_zone(canvas, x, y, w, h, pad=90, blur=60):
    """Fade the background structure back to pure white over a wide halo.

    The white patch extends `pad` design units past the screenshot on every
    side and is blurred hard, so the transition happens well away from the
    image edge and reads as open space rather than a panel.
    """
    px, py = (x - pad) * S, (y - pad) * S
    pw, ph = (w + pad * 2) * S, (h + pad * 2) * S
    mask = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([px, py, px + pw, py + ph],
                                           radius=int(pad * S * 0.9), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(blur * S))
    white = Image.new("RGBA", canvas.size, (255, 255, 255, 255))
    white.putalpha(mask)
    canvas.alpha_composite(white)


def brand(d, x=86, y=76):
    """Small FanFolder wordmark used in the corner of every slide."""
    d.text((x * S, y * S), "FanFolder", font=bold(26), fill=INK)
    d.text((x * S + bold(26).getlength("FanFolder") + 14 * S, y * S + 5 * S),
           "for Windows", font=reg(18), fill=BLUE)


def chips(canvas, labels, x, y, size=19, pad=17, gap=12, h=44):
    """Row of rounded outline chips. Drawn on its own layer so alpha blends."""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    f = bold(size)
    cx = x * S
    for s in labels:
        w = int(f.getlength(s)) + pad * 2 * S
        d.rounded_rectangle([cx, y * S, cx + w, y * S + h * S], radius=h * S // 2,
                            fill=(17, 22, 34, 10), outline=(17, 22, 34, 40), width=S)
        bb = d.textbbox((0, 0), s, font=f)
        d.text((cx + pad * S, y * S + (h * S - (bb[3] - bb[1])) // 2 - bb[1]),
               s, font=f, fill=(70, 82, 102, 255))
        cx += w + gap * S
    canvas.alpha_composite(layer)
    return cx / S


def device(frame_path, target_h, crop=(0, 30, 950, 1830), radius=16, plain=False):
    """Load a captured app frame, crop it, scale it and round its corners.

    plain=True skips the rounding entirely: the capture already has a white
    background, so on the light theme it melts into the canvas with no card.
    """
    src = Image.open(frame_path).convert("RGB").crop(crop)
    cw, ch = src.size
    tw = int(cw * (target_h * S) / ch)
    img = src.resize((tw, target_h * S), Image.LANCZOS).convert("RGBA")
    if plain:
        # Feather the outer edge so the capture dissolves into the white
        # canvas instead of ending on a hard rectangular line.
        f = 10 * S
        mask = Image.new("L", img.size, 0)
        ImageDraw.Draw(mask).rectangle([f, f, img.size[0] - 1 - f, img.size[1] - 1 - f],
                                       fill=255)
        img.putalpha(mask.filter(ImageFilter.GaussianBlur(f / 2)))
        return img
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, img.size[0] - 1, img.size[1] - 1],
                                           radius=radius * S, fill=255)
    img.putalpha(mask)
    return img


def paste_device(canvas, img, x, y, radius=16, shadow=True, border=True):
    """Drop the device onto the canvas. On the light theme the screenshot sits
    directly on the white background, so shadow and border are both optional."""
    px, py = x * S, y * S
    if shadow:
        sh = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle(
            [px + 6 * S, py + 14 * S, px + img.size[0] + 6 * S, py + img.size[1] + 14 * S],
            radius=radius * S, fill=(17, 22, 34, 60))
        canvas.alpha_composite(sh.filter(ImageFilter.GaussianBlur(22 * S)))
    canvas.paste(img, (px, py), img)
    if border:
        ImageDraw.Draw(canvas).rounded_rectangle(
            [px, py, px + img.size[0] - 1, py + img.size[1] - 1],
            radius=radius * S, outline=(17, 22, 34, 38), width=S)


def save(canvas, name):
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, name)
    canvas.convert("RGB").save(p, quality=95)
    print(p, canvas.size)
    return p
