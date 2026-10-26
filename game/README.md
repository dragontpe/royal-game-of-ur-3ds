# Royal Game of Ur engine and AI

Portable C99, no heap, no globals except `const` tables. Files:

- `ur_rules.h/.c` - position, move generation, move application, RNG, board-cell mapping.
- `ur_ai.h/.c` - static eval and expectiminimax AI (three levels).

Host tests and tools live in `host/` and `tools/xcheck/` (see "Running the tests").

## Rules (Finkel, as played on RoyalUr.net), on two boards

Same rules on both boards; only the track differs. `UrPos.board` selects it (`UR_BOARD_UR` or
`UR_BOARD_TWENTY`, set by `ur_init`); everything else asks `UR_BOARDS[board]`.

| | Ur (board 0) | Twenty Squares (board 1, "aseb") |
|---|---|---|
| Track length | 14 (steps 1..14, off = 15) | 16 (steps 1..16, off = 17) |
| Private steps | 1-4, 13-14 | 1-4 |
| Shared steps | 5-12 | 5-16 |
| Rosettes (extra roll) | 4, 8, 14 | 4, 8, 12, 16 |
| Safe (shared rosettes) | 8 | 8, 12, 16 |
| Columns | 8 | 12 |

- 2 players, 7 pieces each, 4 binary dice. Roll 0..4 with weights 1,4,6,4,1 out of 16.
- Step 0 = start pool, 1..track = on the board, track + 1 = borne off. Player A's step s on a shared square is player B's step s.
- A move takes the piece on step s (0 = enter from the pool, one move for all pool pieces) to s + roll. Illegal if: roll is 0; s + roll > track + 1 (exact bear-off); the destination holds your own piece; the destination is a safe square holding an enemy piece.
- Landing on an enemy piece on a shared, non-safe square sends it back to its pool.
- Landing on a rosette gives another roll. Roll 0, or no legal move, passes the turn. If any move exists you must move (capture is not forced).
- No extra roll for capture or bear-off. If an extra roll has no legal move the turn passes.
- First to bear off all 7 wins. Who starts is up to the caller (`ur_init(p, first, board)`).

The Twenty Squares geometry and path are RoyalUr's `AsebBoardShape` / `AsebPathPair`. RoyalUr's own
aseb preset uses 5 pieces; we use 7 (the Finkel count), see the cross-check note below.

## API (`ur_rules.h`)

```c
enum { UR_BOARD_UR = 0, UR_BOARD_TWENTY = 1, UR_BOARD_COUNT };
typedef struct { const char *name; int track, shared_lo, shared_hi; uint32_t rosettes, safe; int cols; } UrBoardDef;
extern const UrBoardDef UR_BOARDS[UR_BOARD_COUNT];

typedef struct { uint32_t mask[2]; uint8_t pool[2], done[2], turn, board; } UrPos;   // 20 bytes, copy by value
typedef struct { uint8_t from, to; } UrMove;      // from 0 = enter, to = track + 1 bears off
typedef struct { bool captured, extra_turn, borne_off; int8_t winner;
                 uint8_t from, to, player; } UrMoveResult;

void ur_init(UrPos *p, int first_player, int board);
int  ur_off(const UrPos *p);                      // track + 1
bool ur_is_rosette(int board, int step);  bool ur_is_shared(int board, int step);  bool ur_is_safe(int board, int step);
int  ur_gen_moves(const UrPos *p, int roll, UrMove out[UR_MAX_MOVES]);  // count, ascending from-step
UrMoveResult ur_apply(UrPos *p, UrMove m, int roll);   // switches turn unless extra_turn
void ur_pass(UrPos *p);                                // roll 0 / no legal move
int  ur_winner(const UrPos *p);                        // -1, 0 or 1
bool ur_pos_valid(const UrPos *p);                     // invariants, for tests/asserts

void ur_rng_seed(UrRng *r, uint64_t seed);             // xorshift64*, splitmix seeding
int  ur_roll(UrRng *r);                                // 0..4, four coin flips
uint32_t ur_rng_below(UrRng *r, uint32_t n);

bool ur_cell_exists(int board, int row, int col);
bool ur_step_cell(int board, int player, int step, int *row, int *col);  // false for pool / off
int  ur_cell_step(int board, int player, int row, int col);              // -1 if not on that track
```

`mask[p]` bit s (1..track) is set when player p has a piece on step s. `UR_MAX_MOVES` is 7,
`UR_MAX_TRACK` 16. `ur_apply` does not validate: pass it a move from `ur_gen_moves`. In
`UrMoveResult`, when `captured` is true the victim sat on the shared square `to`. After a winning move
`turn` has already switched (RoyalUr's own win state has no turn; the cross-check ignores it).

Typical turn:

```c
int roll = ur_roll(&rng);
UrMove mv[UR_MAX_MOVES];
int n = ur_gen_moves(&pos, roll, mv);
if (n == 0) { ur_pass(&pos); }
else {
    UrMove m = (human) ? chosen_by_player : ur_ai_choose(&pos, roll, UR_EXPERT, &rng, NULL);
    UrMoveResult r = ur_apply(&pos, m, roll);   // animate r.from -> r.to, r.captured, r.extra_turn
}
```

Cell mapping: rows 0..2, cols 0..`cols`-1. Player 0 uses row 2, player 1 row 0. For a player on row r:
steps 1-4 = (r,3),(r,2),(r,1),(r,0); shared steps from 5 = (1,0),(1,1)...
- Ur: steps 5-12 = (1,0)..(1,7); step 13 = (r,7); step 14 = (r,6). Rosette cells (r,0), (1,3), (r,6).
- Twenty: steps 5-16 = (1,0)..(1,11). Cells that exist: rows 0 and 2 cols 0-3, row 1 cols 0-11 (20 cells).
  Rosette cells (0,0), (2,0), (1,3), (1,7), (1,11).

## AI (`ur_ai.h`)

```c
UrMove ur_ai_choose(const UrPos *p, int roll, UrLevel level, UrRng *rng, UrAiStats *stats_or_null);
UrMove ur_ai_choose_cfg(..., const UrAiConfig *cfg, ...);   // explicit tuning (UR_AI_DEFAULT)
```

Call only when `ur_gen_moves` returned at least one move. A forced move (one legal move) is returned
at once with 0 nodes at every level.

Search: expectiminimax. A chance node averages the 5 roll outcomes (weights 1,4,6,4,1; 0 and
no-legal-move pass the turn); max/min nodes follow `turn`, so extra rolls need no special case.
Depth counts moves. Alpha-beta with star1 pruning on the chance nodes plus a cheap move ordering
(bear-off, capture, rosette first); a test checks the pruned value equals the plain full-width value
(max difference 0 over 120 positions/depths). Node = one chance-node visit, including leaf evals.

Static eval (from one side's view, difference of both sides): progress per step is `step * 14 / track`
(10 on Ur, 8.75 on Twenty, so a full trip is worth the same on both boards; a borne-off piece counts
track + 1 steps) + 20 per borne-off piece + 45 (`centre`) for each piece on a safe square (Ur: step 8;
Twenty: 8, 12, 16), minus 15 per piece still in the pool, minus expected capture loss: for each piece
on a shared non-safe square, the probability that an enemy piece 1-4 steps behind rolls the right
number times (step weight * step + 15). On Ur this is identical to the pre-Twenty eval. Weights are in `UrEvalW`
(defaults were tuned by self-play at a 3000-node budget: about 55% against the first guess).

| Level | Behaviour |
|---|---|
| `UR_BEGINNER` | 30% of moves picked at random; otherwise greedy 1-ply on the static eval with +-120 noise per move (12 steps of progress), so it often misses captures and leaves pieces exposed. Beats a uniformly random mover 87% of games. |
| `UR_MEDIUM` | Depth 2 (full window), +-20 noise on each root value. |
| `UR_EXPERT` | Iterative deepening, depth 1, 2, 3... until the next depth would not fit in `expert_budget` (100,000 nodes). Result of the last completed depth. |

## Running the tests

```sh
make -C host test      # unit tests, fuzz (both boards), AI checks (~2 min)
make -C host ladder    # self-play ladder, both boards, 2000 games per pairing (~15 min)
make -C host arm       # compile game/*.c with devkitARM (warnings only)
make -C host xcheck    # cross-check both boards against royalur-python (needs tools/xcheck/.venv)
build/ladder <A> <B> <games> [seed] [board=0|1] [a.key=val b.key=val ...]   # from host/, e.g. ladder expert medium 2000 101 board=1
```

Cross-check: `tools/xcheck/gen.c` (args: games, seed, board) plays random and AI-driven games and writes a
transcript (legal moves, chosen move, resulting state per roll). `tools/xcheck/check.py` replays it in
`royalur` and compares legal moves and state after every roll. Ur uses `Game.create_finkel()` (7 pieces,
4 binary dice, Bell path, safe rosettes, extra roll on rosettes only). Twenty uses the library's aseb
settings (`BoardType.ASEB` shape, `PathType.ASEB` paths, same dice and rosette flags) BUT with 7 starting
pieces: `Game.create_aseb()` / `GameSettings.create_aseb()` hard-code 5 pieces, and every other setting
matches our rules. `run.sh` runs both boards. Setup: `python3 -m venv tools/xcheck/.venv &&
tools/xcheck/.venv/bin/pip install royalur` (`run.sh` does this if the venv is missing).
royalur 0.0.6 note: `RuleSet.paths` has no `return`, so `check.py` reads `rules._paths`.
The library always lets LIGHT (player 0) start, so transcripts always start with player 0.

## Measured numbers (Mac, Apple Silicon, clang -O2)

Ladder: sides and first mover alternate; 600 games per pairing per board (the earlier Ur-only run was
2000 games: Expert vs Medium 68.1%, Medium vs Beginner 98.2%, Expert vs Beginner 99.4%, Beginner vs
uniform random 87.2%). The Ur eval is unchanged, so Ur is the same engine; the 600-game rerun below
agrees within its CI. Eval weights were NOT retuned for Twenty Squares.

| Board | Match | First wins | 95% CI (Wilson) |
|---|---|---|---|
| Ur | Expert vs Medium | 68.5% (411/600) | 64.7-72.1% |
| Ur | Medium vs Beginner | 98.0% (588/600) | 96.5-98.9% |
| Ur | Expert vs Beginner | 99.7% (598/600) | 98.8-99.9% |
| Twenty | Expert vs Medium | 70.7% (424/600) | 66.9-74.2% |
| Twenty | Medium vs Beginner | 98.2% (589/600) | 96.7-99.0% |
| Twenty | Expert vs Beginner | 99.3% (596/600) | 98.3-99.7% |

Nodes per move (only moves with a real choice; Expert times are inflated by 6 runs in parallel):

| Level | Board | min | avg | max | avg depth |
|---|---|---|---|---|---|
| Expert | Ur | 6253 | ~50,000 | 100,000 | 4.9 |
| Expert | Twenty | 6289 | ~46,000-52,000 | 100,000 | 4.7-4.8 |
| Medium | Ur | 10 | 36-38 | 105 | 2 |
| Medium | Twenty | 10 | 42-44 | 120 | 2 |
| Beginner | both | 0 | 0 | 0 | 0 |

The Mac does about 23 million nodes/s, so Expert's 100k budget is ~4 ms here. On the 3DS plan for
roughly 1 s worst case at 100k nodes/s; lower `expert_budget` in `UrAiConfig` if hardware measures slower.
