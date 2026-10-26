"""Shapes shared by the comps and the in-game sprites: the flat mini board,
rosettes, square motifs, counters and dice icons."""
import math
from common import *

CELL = 34
BX, BY = 24, 42  # mini board origin

# Light uses row 2, dark row 0 (same as game/ur_rules).
def step_cell(player, step):
    r = 2 if player == 0 else 0
    if 1 <= step <= 4:
        return r, 4 - step
    if 5 <= step <= 12:
        return 1, step - 5
    return r, 7 if step == 13 else 6


ROSETTES = {(0, 0), (2, 0), (1, 3), (0, 6), (2, 6)}
MISSING = {(0, 4), (0, 5), (2, 4), (2, 5)}


def rosette(c, cx, cy, r, petal_a, petal_b, centre):
    """Eight oval petals alternating two colours, like the board's inlay."""
    for k in range(8):
        a = k * math.pi / 4 + math.pi / 8
        col = petal_a if k % 2 == 0 else petal_b
        mx, my = cx + math.cos(a) * r * 0.55, cy + math.sin(a) * r * 0.55
        pts = []
        for i in range(24):
            t = i * 2 * math.pi / 24
            u, v = math.cos(t) * r * 0.45, math.sin(t) * r * 0.17
            pts.append((mx + u * math.cos(a) - v * math.sin(a), my + u * math.sin(a) + v * math.cos(a)))
        c.poly(pts, col)
    c.circle(cx, cy, r * 0.2, centre)


MOTIFS = [
    ['rose', 'eyes', 'five', 'eyes', None, None, 'rose', 'star'],
    ['grid', 'five', 'four', 'rose', 'five', 'four', 'eyes', 'five'],
    ['rose', 'eyes', 'five', 'eyes', None, None, 'rose', 'star'],
]


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def motif(c, kind, x, y, s, square):
    """Muted, simplified versions of the board's square patterns."""
    m = mix(square, LAPIS, 0.38)
    cx, cy = x + s / 2, y + s / 2
    if kind == 'five':
        for dx, dy in ((0, 0), (-1, -1), (1, -1), (-1, 1), (1, 1)):
            c.circle(cx + dx * s * 0.27, cy + dy * s * 0.27, s * 0.075, m)
    elif kind == 'eyes':
        for dx in (-1, 1):
            for dy in (-1, 1):
                px, py, h = cx + dx * s * 0.25, cy + dy * s * 0.24, s * 0.12
                c.poly([(px, py - h), (px + h * 0.75, py), (px, py + h), (px - h * 0.75, py)], m)
                c.circle(px, py, s * 0.035, square)
    elif kind == 'four':
        for dx in (-1, 1):
            for dy in (-1, 1):
                c.circle(cx + dx * s * 0.24, cy + dy * s * 0.24, s * 0.12, m)
                c.circle(cx + dx * s * 0.24, cy + dy * s * 0.24, s * 0.06, square)
    elif kind == 'grid':
        for i in range(4):
            for j in range(4):
                c.circle(x + s * (0.2 + 0.2 * i), y + s * (0.2 + 0.2 * j), s * 0.045, m)
    elif kind == 'star':
        c.circle(cx, cy, s * 0.34, m)
        c.circle(cx, cy, s * 0.27, square)
        for dx, dy in ((0, 0), (-1, -1), (1, -1), (-1, 1), (1, 1)):
            c.circle(cx + dx * s * 0.11, cy + dy * s * 0.11, s * 0.04, m)


def piece(c, cx, cy, r, player, shadow=False):
    if shadow:
        c.circle(cx + 1, cy + 2, r, (*INK, 56))
    body, dot, rim = (SHELL_HI, LAPIS_DEEP, INK) if player == 0 else (INK, SHELL, (14, 10, 8))
    c.circle(cx, cy, r, rim)
    c.circle(cx, cy, r - 1.5, body)
    d = r * 0.45
    for dx, dy in ((0, 0), (-d, -d), (d, -d), (-d, d), (d, d)):
        c.circle(cx + dx, cy + dy, r * 0.13, dot)


def die(c, cx, cy, s, marked, body, mark):
    h = s * 0.866
    c.poly([(cx, cy - h * 0.62), (cx - s / 2, cy + h * 0.38), (cx + s / 2, cy + h * 0.38)], body)
    if marked:
        c.circle(cx, cy - h * 0.62 + s * 0.2, s * 0.13, mark)




def board_map(c, ox, oy, inlay, square, cell=CELL):
    """The static mini board: inlay frame, squares, rosettes, motifs.
    (ox, oy) is the top-left of the first square's cell."""
    c.rect(ox - 3, oy - 3, 4 * cell + 6, 3 * cell + 6, inlay, r=3)
    c.rect(ox + 4 * cell - 3, oy + cell - 3, 2 * cell + 6, cell + 6, inlay)
    c.rect(ox + 6 * cell - 3, oy - 3, 2 * cell + 6, 3 * cell + 6, inlay, r=3)
    for r in range(3):
        for col in range(8):
            if (r, col) in MISSING:
                continue
            x, y = ox + col * cell + 1.5, oy + r * cell + 1.5
            c.rect(x, y, cell - 3, cell - 3, square, r=2)
            if (r, col) in ROSETTES:
                rosette(c, x + (cell - 3) / 2, y + (cell - 3) / 2, 13.5 * cell / CELL, LAPIS, CARNELIAN, CARNELIAN)
            else:
                motif(c, MOTIFS[r][col], x, y, cell - 3, square)
