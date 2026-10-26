"""Bottom-screen direction comps (320x240) for Frank to pick from.

Two flat directions with the same layout:
  A "Lapis": deep lapis ground like the board's inlay, shell text, gold action.
  B "Clay":  sun-dried clay ground like the rules tablet, ink text, lapis action.
Each is drawn in two states (time to roll / pick a move) and stacked under
the real top-down render of the 3D board.
"""
import math
import os
from PIL import Image

from common import *
from shapes import *

def board(c, pal, pieces, targets, movable):
    # inlay frame
    c.rect(BX - 3, BY - 3, 4 * CELL + 6, 3 * CELL + 6, pal['inlay'], r=3)
    c.rect(BX + 4 * CELL - 3, BY + CELL - 3, 2 * CELL + 6, CELL + 6, pal['inlay'])
    c.rect(BX + 6 * CELL - 3, BY - 3, 2 * CELL + 6, 3 * CELL + 6, pal['inlay'], r=3)
    for r in range(3):
        for col in range(8):
            if (r, col) in MISSING:
                continue
            x, y = BX + col * CELL + 1.5, BY + r * CELL + 1.5
            c.rect(x, y, CELL - 3, CELL - 3, pal['square'], r=2)
            if (r, col) in ROSETTES:
                rosette(c, x + (CELL - 3) / 2, y + (CELL - 3) / 2, 13.5, LAPIS, CARNELIAN, CARNELIAN)
            else:
                motif(c, MOTIFS[r][col], x, y, CELL - 3, pal['square'])
    for (r, col) in targets:
        x, y = BX + col * CELL + 1.5, BY + r * CELL + 1.5
        c.rect(x + 2, y + 2, CELL - 7, CELL - 7, None, r=3, outline=pal['target'], width=2.5)
    for player, step in pieces:
        r, col = step_cell(player, step)
        cx, cy = BX + col * CELL + CELL / 2, BY + r * CELL + CELL / 2
        if (r, col) in movable:
            c.circle(cx, cy, 14.5, pal['select'])
        piece(c, cx, cy, 12, player)


def player_row(c, pal, y, player, name, home, active):
    piece(c, 26, y + 13, 9, player)
    c.text(42, y + 18, name, 'poppins', 'SemiBold', 15, pal['text'])
    if active:
        f = font('poppins', 'SemiBold', 15 * SS)
        l, t, r, b = c.d.textbbox((0, 0), name, font=f)
        c.rect(42, y + 23, (r - l) / SS, 3, pal['active'], r=1)
    c.text(258, y + 17, 'Home', 'poppins', 'Medium', 11, pal['soft'], anchor='rs')
    c.rect(264, y + 2, 40, 22, pal['box'], r=3)
    c.text_center_ink(284, y + 13, '%d/7' % home, 'poppins', 'Bold', 13, pal['box_text'])


def bottom(pal, state):
    c = Canvas(320, 240, pal['ground'] + (255,))
    pieces = [(1, 3), (1, 10), (1, 13), (0, 2), (0, 6), (0, 8)]
    if state == 'pick':
        targets = {step_cell(0, 5), step_cell(0, 9), step_cell(0, 11), step_cell(0, 3)}
        movable = {step_cell(0, 2), step_cell(0, 6), step_cell(0, 8)}
    else:
        targets, movable = set(), set()
    player_row(c, pal, 6, 1, 'Expert', 1, False)
    board(c, pal, pieces, targets, movable)
    player_row(c, pal, 148, 0, 'You', 2, True)

    # action row
    if state == 'roll':
        c.text(16, 202, 'Your turn', 'poppins', 'SemiBold', 15, pal['text'])
        c.text(16, 220, 'Tap Roll or press A', 'poppins', 'Medium', 11, pal['soft'])
        c.rect(200, 186, 104, 42, pal['btn'], r=3)
        c.rect(200, 224, 104, 4, pal['btn_lo'], r=0)
        c.text_center_ink(252, 205, 'Roll', 'poppins', 'SemiBold', 18, pal['btn_text'])
    else:
        c.text(16, 202, 'Rolled 3', 'poppins', 'SemiBold', 15, pal['text'])
        c.text(16, 220, 'Tap a piece to move it', 'poppins', 'Medium', 11, pal['soft'])
        for i, m in enumerate((True, True, False, True)):
            die(c, 186 + i * 22, 208, 18, m, pal['die'], pal['die_mark'])
        c.rect(272, 190, 32, 34, pal['box'], r=3)
        c.text_center_ink(288, 207, '3', 'poppins', 'Bold', 22, pal['box_text'])
    return c.native()


LAPIS_PAL = dict(ground=LAPIS_DEEP, inlay=(14, 20, 38), square=SHELL, target=CARNELIAN, select=GOLD,
                 text=SHELL, soft=SHELL_LO, active=GOLD, box=SHELL, box_text=INK,
                 btn=GOLD, btn_lo=GOLD_LO, btn_text=INK, die=INK, die_mark=SHELL_HI)
CLAY_PAL = dict(ground=CLAY, inlay=LAPIS_DEEP, square=SHELL, target=CARNELIAN, select=LAPIS,
                text=INK, soft=INK_SOFT, active=LAPIS, box=SHELL_HI, box_text=INK,
                btn=LAPIS, btn_lo=LAPIS_DEEP, btn_text=SHELL_HI, die=INK, die_mark=SHELL_HI)


def sheet(name, pal, top):
    roll, pick = bottom(pal, 'roll'), bottom(pal, 'pick')
    roll.save(os.path.join(OUT, 'bottom_%s_roll.png' % name))
    pick.save(os.path.join(OUT, 'bottom_%s_pick.png' % name))
    g = Image.new('RGB', (400 + 16 + 320 + 24, 240 * 2 + 24), (30, 30, 34))
    g.paste(top, (8, 8))
    g.paste(pick, (8 + 40, 8 + 240 + 8))
    g.paste(roll, (8 + 400 + 16, 8 + 240 + 8))
    big = g.resize((g.width * 2, g.height * 2), Image.NEAREST)
    big.save(os.path.join(OUT, 'comp_%s_2x.png' % name))


OUT = os.path.join(ROOT, 'design', 'comps')
if __name__ == '__main__':
    top = Image.open(os.path.join(OUT, 'top_topdown_400x240.png')).convert('RGB')
    sheet('A_lapis', LAPIS_PAL, top)
    sheet('B_clay', CLAY_PAL, top)
    print('wrote', OUT)
