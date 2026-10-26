"""Shared palette, fonts and drawing helpers for the Ur 3DS art.

Everything is drawn SS times larger and downsampled to the exact native
pixel size (Gammon-X pipeline), so nothing is scaled at runtime.
"""
import os
from PIL import Image, ImageDraw, ImageFont

SS = 8
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Palette, taken from Frank's board, counters and Babylon room.
LAPIS_DEEP = (24, 33, 58)
LAPIS = (39, 61, 110)
LAPIS_HI = (63, 92, 152)
SHELL = (236, 222, 186)
SHELL_LO = (203, 183, 139)
SHELL_HI = (248, 241, 222)
INK = (40, 30, 24)
INK_SOFT = (110, 92, 76)
CARNELIAN = (196, 84, 47)
GOLD = (218, 165, 62)
GOLD_LO = (172, 122, 38)
CLAY = (205, 172, 128)
CLAY_LO = (180, 146, 104)
CLAY_HI = (222, 196, 158)
WOOD = (79, 56, 36)

FONT_PATHS = {
    'cinzel': '/Library/Fonts/Cinzel-%s.ttf',
    'poppins': '/Library/Fonts/Poppins/Poppins-%s.ttf',
}


def font(face, weight, px):
    path = FONT_PATHS[face] % weight
    if not os.path.exists(path):
        path = os.path.expanduser('~') + path
    return ImageFont.truetype(path, int(round(px)))


class Canvas:
    """A supersampled RGBA canvas addressed in native pixels."""

    def __init__(self, w, h, bg=(0, 0, 0, 0)):
        self.w, self.h = w, h
        self.im = Image.new('RGBA', (w * SS, h * SS), bg)
        self.d = ImageDraw.Draw(self.im)

    def s(self, v):
        return int(round(v * SS))

    def rect(self, x, y, w, h, fill, r=0, outline=None, width=1):
        box = [self.s(x), self.s(y), self.s(x + w) - 1, self.s(y + h) - 1]
        self.d.rounded_rectangle(box, radius=self.s(r), fill=fill,
                                 outline=outline, width=self.s(width) if outline else 0)

    def circle(self, cx, cy, r, fill, outline=None, width=1):
        box = [self.s(cx - r), self.s(cy - r), self.s(cx + r) - 1, self.s(cy + r) - 1]
        self.d.ellipse(box, fill=fill, outline=outline, width=self.s(width) if outline else 0)

    def poly(self, pts, fill):
        self.d.polygon([(self.s(x), self.s(y)) for x, y in pts], fill=fill)

    def ink_bbox(self, text, f):
        return self.d.textbbox((0, 0), text, font=f)

    def text(self, x, y, text, face, weight, px, fill, anchor='ls'):
        """anchor as PIL: l/m/r + a/m/s/d. Coordinates in native pixels."""
        f = font(face, weight, px * SS)
        self.d.text((self.s(x), self.s(y)), text, font=f, fill=fill, anchor=anchor)

    def text_center_ink(self, cx, cy, text, face, weight, px, fill):
        """Centre on the actual ink bounds, not the nominal font box."""
        f = font(face, weight, px * SS)
        l, t, r, b = self.d.textbbox((0, 0), text, font=f)
        self.d.text((self.s(cx) - (l + r) / 2, self.s(cy) - (t + b) / 2), text, font=f, fill=fill)

    def paste(self, img, x, y):
        big = img.resize((img.width * SS, img.height * SS), Image.NEAREST)
        self.im.alpha_composite(big.convert('RGBA'), (self.s(x), self.s(y)))

    def native(self):
        return self.im.resize((self.w, self.h), Image.LANCZOS)


# ---- UI theme: Frank picks A (lapis) or B (clay); everything reads from here ----
THEME = os.environ.get('UR_THEME', 'A')
if THEME == 'A':
    T = dict(ground=LAPIS_DEEP, ground_hi=(33, 45, 78), inlay=(14, 20, 38), text=SHELL, soft=SHELL_LO,
             active=GOLD, tile=SHELL, tile_text=INK, tile_soft=INK_SOFT, box=SHELL, box_text=INK,
             btn=GOLD, btn_lo=GOLD_LO, btn_text=INK, select=GOLD, target=CARNELIAN,
             accent=GOLD, quote=(226, 196, 128), scrim=(10, 14, 28))
else:
    T = dict(ground=CLAY, ground_hi=CLAY_HI, inlay=LAPIS_DEEP, text=INK, soft=INK_SOFT,
             active=LAPIS, tile=SHELL_HI, tile_text=INK, tile_soft=INK_SOFT, box=SHELL_HI, box_text=INK,
             btn=LAPIS, btn_lo=LAPIS_DEEP, btn_text=SHELL_HI, select=LAPIS, target=CARNELIAN,
             accent=LAPIS, quote=(120, 70, 40), scrim=(40, 28, 20))
