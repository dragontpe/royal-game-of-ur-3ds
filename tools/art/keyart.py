"""Splash key art: crop the painted key art (design/keyart/keyart_v1.png,
GPT Image 2) to the top screen's 5:3 and keep 640x384 so the splash can
pan and zoom across it. Packed by tex3ds as an RGB565 sprite sheet."""
import os
from PIL import Image
from common import ROOT

src = Image.open(os.path.join(ROOT, 'design', 'keyart', 'keyart_v1.png')).convert('RGB')
cw = round(src.height * 5 / 3)
x0 = (src.width - cw) // 2
img = src.crop((x0, 0, x0 + cw, src.height)).resize((640, 384), Image.LANCZOS)
img.save(os.path.join(ROOT, 'gfx', 'keyart', 'keyart.png'))
with open(os.path.join(ROOT, 'gfx', 'keyart.t3s'), 'w') as f:
    f.write('--atlas -f rgb565 -z auto\nkeyart/keyart.png\n')
print('keyart 640x384')
