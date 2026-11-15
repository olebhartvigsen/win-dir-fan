"""Render the README intro hero at 2540x1520 (2x the 1270x760 design grid).

Dark composition: the app fans out on a white card, with the real app icon and
the current 1.4 MB size. The Product Hunt slides stay untouched.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import *  # noqa: F401,F403
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
ICON = os.path.join(ROOT, "installer", "directories", "icon-512.png")
if not os.path.exists(ICON):
    ICON = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "icon-512.png")

DARK_TOP = (19, 25, 46)
DARK_BOTTOM = (7, 10, 18)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def dark_background():
    bg = Image.new("RGB", (W, H), DARK_BOTTOM)
    d = ImageDraw.Draw(bg)
    for y in range(H):
        t = (y / (H - 1)) ** 0.92
        d.line([(0, y), (W, y)], fill=lerp(DARK_TOP, DARK_BOTTOM, t))
    dots = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(dots)
    step = 30 * S
    rad = int(1.25 * S)
    for y in range(step, H, step):
        for x in range(step, W, step):
            dd.ellipse([x - rad, y - rad, x + rad, y + rad], fill=(255, 255, 255, 11))
    return Image.alpha_composite(bg.convert("RGBA"), dots)


def radial_glow(canvas, cx, cy, r, color, opacity):
    size = r * 2 * S
    grad = Image.radial_gradient("L").resize((size, size), Image.LANCZOS)
    grad = Image.eval(grad, lambda v: int((255 - v) * opacity))
    glow = Image.new("RGBA", (size, size), color + (0,))
    glow.putalpha(grad)
    canvas.alpha_composite(glow, (cx * S - r * S, cy * S - r * S))


def gradient_text(canvas, xy, text, font, stops):
    mask = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(mask).text(xy, text, font=font, fill=255)
    box = mask.getbbox()
    if not box:
        return
    w, h = box[2] - box[0], box[3] - box[1]
    grad = Image.new("RGB", (w, h))
    gd = ImageDraw.Draw(grad)
    for y in range(h):
        for x in range(w):
            t = (x + y * 0.55) / max(1, w + h * 0.55)
            pos = t * (len(stops) - 1)
            i = min(int(pos), len(stops) - 2)
            f = pos - i
            a, b = stops[i], stops[i + 1]
            gd.point((x, y), (
                int(a[0] + (b[0] - a[0]) * f),
                int(a[1] + (b[1] - a[1]) * f),
                int(a[2] + (b[2] - a[2]) * f),
            ))
    canvas.paste(grad, (box[0], box[1]), mask.crop(box))


def dark_chips(canvas, labels, x, y, size=19, pad=17, gap=12, h=44):
    f = bold(size)
    cx = x
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for s in labels:
        w = int(f.getlength(s)) / S + pad * 2
        d.rounded_rectangle([cx * S, y * S, (cx + w) * S, (y + h) * S],
                            radius=h * S // 2,
                            fill=(255, 255, 255, 18),
                            outline=(255, 255, 255, 58), width=S)
        bb = d.textbbox((0, 0), s, font=f)
        d.text((cx * S + pad * S, y * S + (h * S - (bb[3] - bb[1])) // 2 - bb[1]),
               s, font=f, fill=(255, 255, 255, 255))
        cx += w + gap
    canvas.alpha_composite(layer)


c = dark_background()
radial_glow(c, 1010, 240, 430, (47, 164, 79), 0.22)
radial_glow(c, 900, 560, 380, (47, 107, 216), 0.14)
radial_glow(c, 130, 700, 400, (123, 80, 184), 0.12)

# Brand row.
icon = Image.open(ICON).convert("RGBA").resize((36 * S, 36 * S), Image.LANCZOS)
round_mask = Image.new("L", icon.size, 0)
ImageDraw.Draw(round_mask).rounded_rectangle(
    [0, 0, icon.size[0] - 1, icon.size[1] - 1], radius=8 * S, fill=255)
icon.putalpha(ImageChops.multiply(icon.getchannel("A"), round_mask))
c.alpha_composite(icon, (86 * S, 62 * S))
d = ImageDraw.Draw(c)
d.rounded_rectangle([84 * S, 60 * S, 124 * S, 100 * S], radius=10 * S,
                    outline=(255, 255, 255, 40), width=S)
brand_font = bold(26)
d.text((132 * S, 76 * S), "FanFolder", font=brand_font, fill=(255, 255, 255))
d.text((132 * S + brand_font.getlength("FanFolder") + 14 * S, 76 * S + 5 * S),
       "for Windows", font=reg(18), fill=(127, 176, 255))

# Eyebrow.
eyebrow = Image.new("RGBA", c.size, (0, 0, 0, 0))
ed = ImageDraw.Draw(eyebrow)
ed.rounded_rectangle([86 * S, 216 * S, 424 * S, 252 * S], radius=18 * S,
                     fill=(47, 164, 79, 26), outline=(47, 164, 79, 78), width=S)
ed.ellipse([100 * S, 228 * S, 108 * S, 236 * S], fill=(92, 224, 140, 255))
ed.text((118 * S, 222 * S), "RECENT FILES, ONE CLICK AWAY", font=bold(12),
        fill=(144, 232, 172, 255))
c.alpha_composite(eyebrow)

# Headline.
d.text((86 * S, 300 * S), "Your recent files,", font=bold(64), fill=(255, 255, 255))
gradient_text(c, (86 * S, 380 * S), "one click away", bold(64),
              [(92, 224, 140), (127, 176, 255), (200, 165, 255)])

# Supporting copy.
sub = reg(26)
d.text((86 * S, 496 * S), "Click the taskbar icon and your files fan out.", font=sub,
       fill=(174, 186, 206))
d.text((86 * S, 538 * S), "Open, drag, rename, or delete without leaving what you are doing.", font=sub,
       fill=(174, 186, 206))

dark_chips(c, ["Windows 10 & 11", "1.4 MB", "Free forever"], 86, 620)
d.text((86 * S, 700 * S), "Microsoft Store  ·  winget install FanFolder", font=reg(18),
       fill=(124, 135, 154))

# White card with the actual fan UI.
card_x, card_y, card_w, card_h = 790, 30, 450, 700
shadow = Image.new("RGBA", c.size, (0, 0, 0, 0))
ImageDraw.Draw(shadow).rounded_rectangle(
    [(card_x + 10) * S, (card_y + 22) * S, (card_x + card_w + 10) * S, (card_y + card_h + 22) * S],
    radius=24 * S, fill=(0, 0, 0, 150))
c.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(30 * S)))

card = Image.new("RGBA", (card_w * S, card_h * S), (255, 255, 255, 255))
card_mask = Image.new("L", card.size, 0)
ImageDraw.Draw(card_mask).rounded_rectangle(
    [0, 0, card.size[0] - 1, card.size[1] - 1], radius=24 * S, fill=255)
card.putalpha(card_mask)
c.alpha_composite(card, (card_x * S, card_y * S))

img = device(FRAME, 660, plain=True)
img_x = int(card_x + (card_w - img.size[0] / S) / 2)
img_y = card_y + 20
paste_device(c, img, img_x, img_y, shadow=False, border=False)

d.rounded_rectangle([card_x * S, card_y * S, (card_x + card_w) * S, (card_y + card_h) * S],
                    radius=24 * S, outline=(255, 255, 255, 42), width=S)

out = os.path.join(ROOT, "docs", "readme-hero.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
c.convert("RGB").save(out, quality=95)
print(out, c.size)