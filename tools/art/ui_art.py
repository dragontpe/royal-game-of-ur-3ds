"""In-game 2D sprites at native size: gfx/art/*.png + gfx/art.t3s.
Everything is drawn 8x and downsampled, so nothing scales at runtime."""
import os
from PIL import Image

from common import *
from shapes import *

OUT = os.path.join(ROOT, 'gfx', 'art')
SPRITES = []


def save(name, img):
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name + '.png'))
    SPRITES.append(name)


def sprite(name, w, h, fn):
    c = Canvas(w, h)
    fn(c)
    save(name, c.native())


def corners(name, r, color):
    """Four anti-aliased corner sprites for flat rounded panels."""
    c = Canvas(2 * r, 2 * r)
    c.rect(0, 0, 2 * r, 2 * r, color, r=r)
    im = c.native()
    save('%s_tl' % name, im.crop((0, 0, r, r)))
    save('%s_tr' % name, im.crop((r, 0, 2 * r, r)))
    save('%s_bl' % name, im.crop((0, r, r, 2 * r)))
    save('%s_br' % name, im.crop((r, r, 2 * r, 2 * r)))


def build():
    # mini board: 278 x 108, first cell at (3, 3)
    sprite('map', 8 * CELL + 6, 3 * CELL + 6, lambda c: board_map(c, 3, 3, T['inlay'], SHELL))
    sprite('pc_l', 26, 26, lambda c: piece(c, 13, 13, 12, 0))
    sprite('pc_d', 26, 26, lambda c: piece(c, 13, 13, 12, 1))
    sprite('pc_l_s', 20, 20, lambda c: piece(c, 10, 10, 9, 0))
    sprite('pc_d_s', 20, 20, lambda c: piece(c, 10, 10, 9, 1))
    sprite('ring', 32, 32, lambda c: (c.circle(16, 16, 14.5, T['select']), c.circle(16, 16, 12.2, (0, 0, 0, 0))))
    sprite('ring_sel', 34, 34, lambda c: (c.circle(17, 17, 16, SHELL_HI), c.circle(17, 17, 14.5, T['select']),
                                          c.circle(17, 17, 12.2, (0, 0, 0, 0))))
    sprite('tgt', 31, 31, lambda c: c.rect(2, 2, 27, 27, None, r=3, outline=T['target'], width=2.5))
    sprite('tgt_sel', 31, 31, lambda c: (c.rect(1, 1, 29, 29, (*T['target'], 70), r=4),
                                         c.rect(1, 1, 29, 29, None, r=4, outline=T['target'], width=3.5)))
    sprite('die_m', 20, 18, lambda c: die(c, 10, 10, 18, True, INK, SHELL_HI))
    sprite('die_u', 20, 18, lambda c: die(c, 10, 10, 18, False, INK, SHELL_HI))

    def roll_btn(c):
        c.rect(0, 0, 104, 46, T['btn_lo'], r=3)
        c.rect(0, 0, 104, 42, T['btn'], r=3)
        c.text_center_ink(52, 21, 'Roll', 'poppins', 'SemiBold', 18, T['btn_text'])
    sprite('btn_roll', 104, 46, roll_btn)

    for name, r, col in (('c_tile3', 3, T['tile']), ('c_tile6', 6, T['tile']), ('c_hi6', 6, T['ground_hi']),
                         ('c_btn3', 3, T['btn']), ('c_btnlo3', 3, T['btn_lo']), ('c_box3', 3, T['box']),
                         ('c_scrim6', 6, T['scrim'])):
        corners(name, r, col)

    sprite('rosette_big', 48, 48, lambda c: rosette(c, 24, 24, 22, LAPIS, CARNELIAN, CARNELIAN))
    sprite('rosette_huge', 120, 120, lambda c: rosette(c, 60, 60, 56, LAPIS, CARNELIAN, CARNELIAN))
    sprite('rosette_small', 24, 24, lambda c: rosette(c, 12, 12, 11, LAPIS, CARNELIAN, CARNELIAN))

    def chevron(c, left):
        pts = [(9, 2), (4, 8), (9, 14)] if left else [(3, 2), (8, 8), (3, 14)]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            # thick line as a quad
            import math
            dx, dy = x1 - x0, y1 - y0
            l = math.hypot(dx, dy)
            nx, ny = -dy / l * 1.1, dx / l * 1.1
            c.poly([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], T['tile_text'])
            c.circle(x1, y1, 1.1, T['tile_text'])
            c.circle(x0, y0, 1.1, T['tile_text'])
    sprite('chev_l', 12, 16, lambda c: chevron(c, True))
    sprite('chev_r', 12, 16, lambda c: chevron(c, False))

    # small versions for the rules pages (24 px squares)
    S = 24
    sprite('map_s', 8 * S + 6, 3 * S + 6, lambda c: board_map(c, 3, 3, T['inlay'], SHELL, S))
    sprite('pc_l_xs', 20, 20, lambda c: piece(c, 10, 10, 8.5, 0))
    sprite('pc_d_xs', 20, 20, lambda c: piece(c, 10, 10, 8.5, 1))
    sprite('ring_xs', 24, 24, lambda c: (c.circle(12, 12, 11, T['select']), c.circle(12, 12, 9, (0, 0, 0, 0))))
    sprite('tgt_xs', 21, 21, lambda c: c.rect(1.5, 1.5, 18, 18, None, r=2.5, outline=T['target'], width=2))
    sprite('die_m_l', 34, 30, lambda c: die(c, 17, 17, 30, True, INK, SHELL_HI))
    sprite('die_u_l', 34, 30, lambda c: die(c, 17, 17, 30, False, INK, SHELL_HI))

    # path diagram for the rules page: light's track with arrows
    def path(c, CELL=S):
        import math
        pts = []
        for s in range(1, 15):
            r, col = step_cell(0, s)
            pts.append((3 + col * CELL + CELL / 2, 3 + r * CELL + CELL / 2))
        start = (3 + 4 * CELL + CELL / 2, 3 + 2 * CELL + CELL / 2)
        end = (3 + 5 * CELL + CELL / 2 + 6, 3 + 2 * CELL + CELL / 2)
        pts = [start] + pts + [end]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            dx, dy = x1 - x0, y1 - y0
            l = math.hypot(dx, dy)
            nx, ny = -dy / l * 1.6, dx / l * 1.6
            c.poly([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], (*GOLD, 235))
            c.circle(x1, y1, 1.6, (*GOLD, 235))
        x0, y0 = pts[-2]
        x1, y1 = pts[-1]
        ang = math.atan2(y1 - y0, x1 - x0)
        tip = (x1 + math.cos(ang) * 4, y1 + math.sin(ang) * 4)
        c.poly([tip, (x1 + math.cos(ang + 2.4) * 7, y1 + math.sin(ang + 2.4) * 7),
                (x1 + math.cos(ang - 2.4) * 7, y1 + math.sin(ang - 2.4) * 7)], GOLD)
        c.circle(start[0], start[1], 4, GOLD)
    sprite('path_l_s', 8 * S + 6, 3 * S + 6, path)

    # top-screen logo, legible over the room render
    def logo(c):
        w, h = c.w, c.h
        sub = font('cinzel', 'Bold', 15 * SS)
        big = font('cinzel', 'Black', 44 * SS)
        # soft dark halo
        halo = Canvas(w, h)
        halo.text_center_ink(w / 2, 22, 'THE ROYAL GAME', 'cinzel', 'Bold', 15, (0, 0, 0, 200))
        halo.text_center_ink(w / 2, 58, 'OF UR', 'cinzel', 'Black', 44, (0, 0, 0, 200))
        from PIL import ImageFilter
        blurred = halo.im.filter(ImageFilter.GaussianBlur(5 * SS))
        c.im.alpha_composite(blurred)
        c.im.alpha_composite(blurred)
        c.text_center_ink(w / 2, 22, 'THE ROYAL GAME', 'cinzel', 'Bold', 15, SHELL_HI)
        c.text_center_ink(w / 2 + 0.5, 59.5, 'OF UR', 'cinzel', 'Black', 44, (60, 40, 14))
        c.text_center_ink(w / 2, 58, 'OF UR', 'cinzel', 'Black', 44, GOLD)
        rosette(c, w / 2 - 92, 58, 12, LAPIS, CARNELIAN, CARNELIAN)
        rosette(c, w / 2 + 92, 58, 12, LAPIS, CARNELIAN, CARNELIAN)
    sprite('logo', 300, 88, logo)

    with open(os.path.join(ROOT, 'gfx', 'art.t3s'), 'w') as f:
        f.write('--atlas -f rgba8888 -z auto\n')
        for n in SPRITES:
            f.write('art/%s.png\n' % n)
    print('%d sprites' % len(SPRITES))


if __name__ == '__main__':
    build()
