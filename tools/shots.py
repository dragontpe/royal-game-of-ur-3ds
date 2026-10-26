#!/usr/bin/env python3
"""Convert the game's BMP screenshots (top 400x240 above bottom 320x240)
from the isolated Azahar SD card into PNGs in design/shots/ (1x and 2x)."""
import glob
import os
import sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SD = os.path.join(ROOT, 'tools/az/home/Library/Application Support/Azahar/sdmc/3ds/ur-3ds/shots')
OUT = os.path.join(ROOT, 'design', 'shots')
os.makedirs(OUT, exist_ok=True)
names = sys.argv[1:] or sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(SD, '*.bmp')))
for n in names:
    im = Image.open(os.path.join(SD, n + '.bmp')).convert('RGB')
    im.save(os.path.join(OUT, n + '.png'))
    im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(os.path.join(OUT, n + '_2x.png'))
print(len(names), 'shots ->', OUT)
