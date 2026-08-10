"""Slide 3: point it at any folder. Feature cards, no device."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import *
from PIL import Image, ImageDraw, ImageFilter

c = background(glow_x=635, glow_y=430, glow_r=560, strength=38)
d = ImageDraw.Draw(c)
brand(d)

# centred headline
def centre(text, font, y, fill):
    w = font.getlength(text)
    d.text(((W - w) / 2, y * S), text, font=font, fill=fill)

centre("Point it at any folder", bold(56), 178, WHITE)
centre("Recent files are only the default.", reg(25), 268, DIM)

CARDS = [
    ("Recent documents", "everything you opened\nlately, across every app"),
    ("Downloads", "the folder you dig through\nmore than any other"),
    ("A project folder", "point it at whatever job\nis eating your week"),
    ("Office and M365", "recent cloud documents\nby last opened"),
]

layer = Image.new("RGBA", c.size, (0, 0, 0, 0))
ld = ImageDraw.Draw(layer)
shadow = Image.new("RGBA", c.size, (0, 0, 0, 0))
sd = ImageDraw.Draw(shadow)

CW, CH = 268, 190
GAP = 24
total = len(CARDS) * CW + (len(CARDS) - 1) * GAP
x0 = (1270 - total) / 2
y0 = 368

ht = bold(23)
bt = reg(19)
for i, (title, body) in enumerate(CARDS):
    x = x0 + i * (CW + GAP)
    sd.rounded_rectangle([x * S, (y0 + 5) * S, (x + CW) * S, (y0 + CH + 5) * S],
                         radius=18 * S, fill=(17, 22, 34, 40))
    ld.rounded_rectangle([x * S, y0 * S, (x + CW) * S, (y0 + CH) * S], radius=18 * S,
                         fill=(255, 255, 255, 255), outline=(17, 22, 34, 30), width=S)
    # accent bar
    ld.rounded_rectangle([(x + 28) * S, (y0 + 30) * S, (x + 28 + 40) * S, (y0 + 34) * S],
                         radius=2 * S, fill=BLUE + (255,))
    ld.text(((x + 28) * S, (y0 + 58) * S), title, font=ht, fill=INK + (255,))
    yy = y0 + 100
    for ln in body.split("\n"):
        ld.text(((x + 28) * S, yy * S), ln, font=bt, fill=(96, 107, 126, 255))
        yy += 30

c.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(9 * S)))
c.alpha_composite(layer)

centre("Sort by date or name. Cap the item count. Filter with a regex.",
       reg(23), 610, FAINT)

save(c, "03-any-folder.png")
