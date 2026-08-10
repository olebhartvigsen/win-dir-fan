#!/usr/bin/env python3
"""Render all five Product Hunt gallery slides.

    python3 render_all.py

Writes 01-hero.png ... 05-get-it.png into the parent producthunt/ folder at
2540x1520, which is 2x the Product Hunt gallery size of 1270x760.

Only needs Pillow:  pip3 install Pillow
"""
import os
import runpy
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

for name in ["s1.py", "s2.py", "s3.py", "s4.py", "s5.py"]:
    print("rendering", name)
    runpy.run_path(os.path.join(HERE, name), run_name="__main__")

print("done")
