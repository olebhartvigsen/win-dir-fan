"""Slide 4: make it yours. Themes, sizes, languages."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import *
from PIL import Image, ImageDraw

c = background(glow_x=900, glow_y=400, glow_r=480)
d = ImageDraw.Draw(c)
brand(d)

X = 86
d.text((X * S, 268 * S), "Make it look", font=bold(60), fill=WHITE)
d.text((X * S, 344 * S), "the way you want", font=bold(60), fill=BLUE)

sub = reg(25)
for i, ln in enumerate(["Light or dark. Big icons or small.",
                        "Fan out up, down, left or right."]):
    d.text((X * S, (460 + i * 40) * S), ln, font=sub, fill=DIM)

# stat strip
layer = Image.new("RGBA", c.size, (0, 0, 0, 0))
ld = ImageDraw.Draw(layer)
STATS = [("29", "languages"), ("4", "directions"), ("161 KB", "installed")]
sx = X
for big, small in STATS:
    fb_, fs_ = bold(38), reg(19)
    ld.text((sx * S, 566 * S), big, font=fb_, fill=WHITE + (255,))
    ld.text((sx * S, 618 * S), small, font=fs_, fill=(150, 161, 182, 255))
    sx += max(fb_.getlength(big), fs_.getlength(small)) / S + 70
c.alpha_composite(layer)

chips(c, ["English", "Deutsch", "Français", "Español", "Dansk", "+24 more"], X, 690, size=17, h=40)

# the app, single device, sitting confidently on the right
img = device(FRAME, 620, plain=True)
clear_zone(c, 800, 84, img.size[0] / S, 620)
paste_device(c, img, 800, 84, shadow=False, border=False)

save(c, "04-customise.png")
