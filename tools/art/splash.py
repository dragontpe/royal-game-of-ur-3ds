"""No Banners Software splash: two direction comps (top 400x240 + bottom 320x240).

S1 "Empty pole": a slim gold flagpole with its halyard and no flag.
S2 "Missing banner": the dashed outline of a swallowtail banner that isn't there.
Same wordmark and copyright line in both.
"""
import math
import os
from PIL import Image, ImageFilter

from common import *

DARK = (16, 14, 18)
DARK_HI = (34, 30, 36)
WORD = SHELL_HI
OUT = os.path.join(ROOT, 'design', 'comps')


def ground(c, w, h):
    """Dark ground with a smooth radial light pool behind the mark."""
    import numpy as np
    W, H = w * SS, h * SS
    y, x = np.mgrid[0:H, 0:W]
    d = np.sqrt(((x - W / 2) / (W * 0.42)) ** 2 + ((y - H * 0.47) / (H * 0.55)) ** 2)
    t = np.clip(1 - d, 0, 1) ** 1.8
    img = np.zeros((H, W, 4), dtype=np.uint8)
    for i in range(3):
        img[..., i] = (DARK[i] + (DARK_HI[i] - DARK[i]) * t).astype(np.uint8)
    img[..., 3] = 255
    c.im.paste(Image.fromarray(img, 'RGBA'), (0, 0))


WORD_W = None


def word_width():
    f = font('poppins', 'SemiBold', 30 * SS)
    return f.getlength('No Banners') / SS


def wordmark(c, x, y):
    c.text(x, y, 'No Banners', 'poppins', 'SemiBold', 30, WORD, anchor='ls')
    f = font('poppins', 'Medium', 10.5 * SS)
    # letter-spaced SOFTWARE under the wordmark, left-aligned with it
    px = c.s(x) + 2 * SS
    for ch in 'SOFTWARE':
        c.d.text((px, c.s(y + 16)), ch, font=f, fill=GOLD, anchor='ls')
        px += f.getlength(ch) + 3.2 * SS


def pole(c, x, top, bottom, rope=True):
    c.rect(x - 1.3, top, 2.6, bottom - top, GOLD, r=1.2)
    c.circle(x, top - 2.5, 3.6, GOLD)
    c.rect(x - 6, bottom - 1.5, 12, 3, GOLD_LO, r=1.5)
    if rope:
        c.rect(x + 3.2, top + 2, 0.9, bottom - top - 16, (*SHELL, 150))
        c.rect(x + 1.2, bottom - 26, 4, 1.6, GOLD, r=0.8)


def dashed_banner(c, x, top):
    """Swallowtail banner outline in short dashes, hung from the pole at x."""
    w, h, notch = 54, 34, 12
    pts = [(x + 2, top + 2), (x + 2 + w, top + 2), (x + 2 + w - notch, top + 2 + h / 2),
           (x + 2 + w, top + 2 + h), (x + 2, top + 2 + h)]
    pts.append(pts[0])
    dash, gap = 4.0, 3.2
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        L = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        t = 0
        while t < L:
            e = min(L, t + dash)
            ax, ay, bx, by = x0 + ux * t, y0 + uy * t, x0 + ux * e, y0 + uy * e
            nx, ny = -uy * 0.75, ux * 0.75
            c.poly([(ax + nx, ay + ny), (bx + nx, by + ny), (bx - nx, by - ny), (ax - nx, ay - ny)], (*SHELL, 200))
            t = e + gap


def top_s1():
    c = Canvas(400, 240)
    ground(c, 400, 240)
    gw = 18 + word_width()             # pole (6) + gap (12) + wordmark
    x0 = 200 - gw / 2
    pole(c, x0 + 3, 84, 160)
    wordmark(c, x0 + 18, 128)
    return c.native()


def top_s2():
    c = Canvas(400, 240)
    ground(c, 400, 240)
    gw = 72 + word_width()             # pole + banner outline (60) + gap (12) + wordmark
    x0 = 200 - gw / 2
    pole(c, x0 + 3, 78, 162, rope=False)
    dashed_banner(c, x0 + 4, 80)
    wordmark(c, x0 + 72, 128)
    return c.native()


def bottom():
    c = Canvas(320, 240)
    c.rect(0, 0, 320, 240, DARK)
    c.text_center_ink(160, 214, '© 2026 No Banners Software', 'poppins', 'Medium', 11, SHELL_LO)
    return c.native()


def comp(name, top):
    b = bottom()
    top.save(os.path.join(OUT, 'splash_%s_top.png' % name))
    b.save(os.path.join(OUT, 'splash_%s_bottom.png' % name))
    g = Image.new('RGB', (416, 240 * 2 + 24), (30, 30, 34))
    g.paste(top, (8, 8))
    g.paste(b, (48, 256))
    g.resize((g.width * 2, g.height * 2), Image.NEAREST).save(os.path.join(OUT, 'comp_splash_%s_2x.png' % name))


if __name__ == '__main__':
    comp('S1_pole', top_s1())
    comp('S2_missing_banner', top_s2())
    print('splash comps in', OUT)
