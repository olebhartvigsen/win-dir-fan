"""Slide 1: hero. The headline plus the app open on the taskbar."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import *
from PIL import ImageDraw

c = background(glow_x=900, glow_y=380, glow_r=470)
d = ImageDraw.Draw(c)

brand(d)

X = 86
d.text((X * S, 300 * S), "Your recent files,", font=bold(64), fill=WHITE)
d.text((X * S, 380 * S), "one click away", font=bold(64), fill=BLUE)

sub = reg(26)
d.text((X * S, 496 * S), "Click the icon on your taskbar.", font=sub, fill=DIM)
d.text((X * S, 538 * S), "Everything you opened lately fans out.", font=sub, fill=DIM)

chips(c, ["Windows 10 & 11", "161 KB", "Free forever"], X, 620)

img = device(FRAME, 660, plain=True)
clear_zone(c, 800, 60, img.size[0] / S, 660)
paste_device(c, img, 800, 60, shadow=False, border=False)

save(c, "01-hero.png")
