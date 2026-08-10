"""Slide 2: the right-click menu. Real Windows actions, no new UI to learn."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import *
from PIL import ImageDraw

c = background(glow_x=380, glow_y=400, glow_r=470)
d = ImageDraw.Draw(c)

# device on the LEFT this time so the gallery alternates
img = device(FRAME, 620, plain=True)
clear_zone(c, 120, 84, img.size[0] / S, 620)
paste_device(c, img, 120, 84, shadow=False, border=False)

X = 600
brand(d, x=X, y=110)

d.text((X * S, 254 * S), "Every Windows", font=bold(54), fill=WHITE)
d.text((X * S, 322 * S), "action you know", font=bold(54), fill=BLUE)

sub = reg(24)
lines = [
    "Right click any item for the real Windows",
    "context menu. Open, copy, rename, delete,",
    "properties. Nothing new to learn.",
]
y = 424
for ln in lines:
    d.text((X * S, y * S), ln, font=sub, fill=DIM)
    y += 38

# feature bullets
bl = bold(22)
br = reg(22)
items = [
    ("Open", "single click, straight into the app"),
    ("Drag out", "drop a file into mail or chat"),
    ("Drag in", "drop from Explorer to move it here"),
]
y = 566
for head, tail in items:
    d.ellipse([X * S, y * S + 9 * S, X * S + 10 * S, y * S + 19 * S], fill=BLUE)
    d.text((X * S + 26 * S, y * S), head, font=bl, fill=WHITE)
    d.text((X * S + 26 * S + bl.getlength(head) + 12 * S, y * S), tail, font=br, fill=FAINT)
    y += 44

save(c, "02-actions.png")
