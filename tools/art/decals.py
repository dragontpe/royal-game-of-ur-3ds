"""Decal atlas for the 3D board: white shapes with alpha, tinted at draw time.
128x128, four 64x64 cells: 0 soft blob shadow, 1 ring, 2 rounded-square frame, 3 soft glow disc."""
import math, os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SS = 4
N = 64 * SS
im = Image.new('RGBA', (128, 128), (255, 255, 255, 0))


def cell(fn):
    big = Image.new('L', (N, N), 0)
    px = big.load()
    for y in range(N):
        for x in range(N):
            u, v = (x + 0.5) / N * 2 - 1, (y + 0.5) / N * 2 - 1
            px[x, y] = int(max(0, min(1, fn(u, v))) * 255)
    return big.resize((64, 64), Image.LANCZOS)


def blob(u, v):
    r = math.hypot(u, v)
    return max(0.0, 1 - r) ** 1.6


def ring(u, v):
    r = math.hypot(u, v)
    return 1.0 if 0.80 <= r <= 0.96 else 0.0


def frame(u, v):
    # rounded square outline, corner radius 0.2, stroke 0.11
    def sd(u, v, half, rad):
        qx, qy = abs(u) - half + rad, abs(v) - half + rad
        return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - rad
    d = sd(u, v, 0.92, 0.2)
    return 1.0 if -0.13 <= d <= 0 else 0.0


def glow(u, v):
    r = math.hypot(u, v)
    return max(0.0, 1 - r) ** 0.8


for i, fn in enumerate((blob, ring, frame, glow)):
    a = cell(fn)
    white = Image.new('RGBA', (64, 64), (255, 255, 255, 0))
    white.putalpha(a)
    im.paste(white, ((i % 2) * 64, (i // 2) * 64))
im.save(os.path.join(ROOT, 'gfx', 'decals.png'))
print('gfx/decals.png')
