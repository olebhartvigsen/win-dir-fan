"""Slide 5: the closer. Privacy, price, and where to get it."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import *
from PIL import Image, ImageDraw

c = background(glow_x=635, glow_y=360, glow_r=580, strength=40)
d = ImageDraw.Draw(c)
brand(d)


def centre(text, font, y, fill):
    d.text(((W - font.getlength(text)) / 2, y * S), text, font=font, fill=fill)


centre("Free, tiny, and yours", bold(58), 190, WHITE)
centre("No account. No subscription. No data leaves your PC.", reg(25), 282, DIM)

layer = Image.new("RGBA", c.size, (0, 0, 0, 0))
ld = ImageDraw.Draw(layer)

POINTS = [
    ("Nothing to sign up for", "install it and it works"),
    ("Runs on your machine", "your file list never gets uploaded"),
    ("161 KB on disk", "smaller than most photos"),
    ("Open source, MIT", "read the code yourself"),
]

# two columns of check rows
ht = bold(24)
bt = reg(20)
COL_X = [190, 690]
for i, (title, body) in enumerate(POINTS):
    x = COL_X[i % 2]
    y = 380 + (i // 2) * 108
    # check mark in a soft blue disc
    ld.ellipse([x * S, y * S, (x + 34) * S, (y + 34) * S], fill=(28, 110, 226, 30))
    ld.line([(x + 10) * S, (y + 17) * S, (x + 15) * S, (y + 23) * S],
            fill=BLUE + (255,), width=3 * S)
    ld.line([(x + 15) * S, (y + 23) * S, (x + 25) * S, (y + 11) * S],
            fill=BLUE + (255,), width=3 * S)
    ld.text(((x + 52) * S, (y - 2) * S), title, font=ht, fill=WHITE + (255,))
    ld.text(((x + 52) * S, (y + 32) * S), body, font=bt, fill=(96, 107, 126, 255))

c.alpha_composite(layer)

centre("Get it from", reg(21), 600, FAINT)

# install option chips, centred
labels = ["Microsoft Store", "winget", "Direct download"]
f = bold(21)
widths = [f.getlength(s) / S + 40 for s in labels]
start = (1270 - (sum(widths) + 12 * (len(labels) - 1))) / 2
chips(c, labels, start, 640, size=21, pad=20, h=50)

save(c, "05-get-it.png")
