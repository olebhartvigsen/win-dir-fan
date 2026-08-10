# Product Hunt gallery

Launch assets for the FanFolder Product Hunt page.

## Files

| File | Size | Slide |
|---|---|---|
| `01-hero.png` | 2540x1520 | Your recent files, one click away |
| `02-actions.png` | 2540x1520 | Every Windows action you know |
| `03-any-folder.png` | 2540x1520 | Point it at any folder |
| `04-customise.png` | 2540x1520 | Make it look the way you want |
| `05-get-it.png` | 2540x1520 | Free, tiny, and yours |
| `fanfolder-demo.gif` | 1200x750 | Animated demo, the fan opening |
| `fanfolder-demo.mp4` | 1200x750 | Same demo as video |

Slides render at 2540x1520, which is 2x the Product Hunt gallery size of
1270x760, so they stay sharp on retina displays.

## Regenerating

```
pip3 install Pillow
cd scripts
python3 render_all.py
```

Output lands in this folder. `render_all.py` runs `s1.py` through `s5.py` in
order, and each slide can also be rendered on its own.

## How it fits together

`scripts/lib.py` holds the shared pieces: the canvas, fonts, colour palette,
chips, and the screenshot placement helpers. Each `sN.py` is one slide.

`scripts/frames/app-frame.png` is a still pulled from `menu-opens.mp4`. The
app was recorded with Danish UI text, so the top pill was repainted to read
"Open in Explorer" before it went into the gallery.

Two details worth keeping if you edit these:

**The canvas and the screenshot share the same white.** The captures have a
pure 255,255,255 background. If the canvas uses any gradient or tint behind a
screenshot, the difference shows up as a faint rectangle around it. That is
why `clear_zone()` exists.

**Background structure is added, then erased around each screenshot.** The
dot grid and the cool wash cover the whole canvas. `clear_zone()` then fades
them back to pure white in a wide, heavily blurred halo that reaches 90 design
units past the image on every side. The transition lands in open space, so no
edge forms and the screenshot reads as floating rather than boxed.
