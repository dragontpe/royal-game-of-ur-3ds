#!/usr/bin/env python3
"""Replay a transcript from xcheck_gen in RoyalUr's Python library (Finkel settings)
and compare legal moves and the resulting state at every roll."""
import sys
from royalur import Game, PlayerType
from royalur.rules.state import WinGameState

P = [PlayerType.LIGHT, PlayerType.DARK]   # C player 0 = LIGHT, 1 = DARK


def make_game(board):
    if board == 0:
        return Game.create_finkel()
    # The library's own aseb preset, unmodified: aseb shape and paths, 4 binary dice, safe rosettes,
    # extra roll on rosettes, none on capture, 5 starting pieces.
    return Game.create_aseb()


OFF = {0: 15, 1: 17}


def step_of(move, off):
    """(from, to) in our step numbering."""
    frm = 0 if move.is_introducing_piece() else move.get_source_piece().path_index + 1
    to = off if move.is_scoring_piece() else move.get_dest_piece().path_index + 1
    return frm, to


def lib_state(game):
    st = game.get_current_state()
    board = game.get_board()
    paths = game.rules._paths   # royalur 0.0.6: the public `paths` property has no return statement
    masks = []
    for pl in P:
        m = 0
        for i, tile in enumerate(paths.get(pl)):
            piece = board.get(tile)
            if piece is not None and piece.owner == pl and piece.path_index == i:
                m |= 1 << (i + 1)
        masks.append(m)
    lp, dp = game.get_light_player(), game.get_dark_player()
    if isinstance(st, WinGameState):
        winner = 0 if st.get_winner() == PlayerType.LIGHT else 1
        turn = -1   # turn is meaningless once won; not compared
    else:
        winner = -1
        turn = 0 if st.get_turn() == PlayerType.LIGHT else 1
    return (turn, masks[0], masks[1], lp.piece_count, dp.piece_count, lp.score, dp.score, winner)


def main(path):
    games = turns = moves = 0
    board = 0
    off = 15
    mismatches = []
    game = None
    skip_game = False
    first_state = True
    with open(path) as f:
        for ln, line in enumerate(f, 1):
            tok = line.split()
            if not tok:
                continue
            if tok[0] == "G":
                games += 1
                first = int(tok[2])
                board = int(tok[4])
                off = OFF[board]
                game = make_game(board)
                skip_game = False
                assert first == 0, "library games always start with LIGHT"
                first_state = True
            elif tok[0] == "S":
                if skip_game:
                    continue
                want = tuple(int(x) for x in tok[1:])
                got = lib_state(game)
                if want[7] >= 0:
                    want = (-1,) + want[1:]
                if got != want:
                    mismatches.append((ln, "state", want, got))
            elif tok[0] == "M":
                if skip_game:
                    continue
                player, roll, n = int(tok[1]), int(tok[2]), int(tok[3])
                ours = tok[4:4 + n]
                chosen = tok[4 + n]
                turns += 1
                st = game.get_current_state()
                libturn = 0 if st.get_turn() == PlayerType.LIGHT else 1
                if libturn != player:
                    mismatches.append((ln, "turn", player, libturn))
                    skip_game = True
                    continue
                game.roll_dice(roll)
                if game.is_waiting_for_move():
                    avail = sorted(step_of(m, off) for m in game.find_available_moves())
                else:
                    avail = []
                mine = sorted(tuple(int(x) for x in s.split(":")) for s in ours)
                if avail != mine:
                    mismatches.append((ln, "moves", mine, avail))
                    skip_game = True
                    continue
                if chosen != "pass":
                    frm, to = (int(x) for x in chosen.split(":"))
                    for m in game.find_available_moves():
                        if step_of(m, off) == (frm, to):
                            game.make_move(m)
                            break
                    moves += 1
    print(f"board: {'Ur (Finkel)' if board == 0 else 'Twenty Squares (RoyalUr aseb preset, 5 pieces)'}")
    print(f"games compared: {games}")
    print(f"rolls compared: {turns} (moves played {moves}, passes {turns - moves})")
    print(f"mismatches: {len(mismatches)}")
    for m in mismatches[:10]:
        print("  line %d %s: C=%s lib=%s" % m)
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
