# Royal Game of Ur engine and AI

Portable C99, no heap, no globals except `const` tables. Files:

- `ur_rules.h/.c` - position, move generation, move application, RNG, board-cell mapping.
- `ur_ai.h/.c` - static eval and expectiminimax AI (three levels).

Host tests and tools live in `host/` and `tools/xcheck/` (see "Running the tests").

## Rules (Finkel, as played on RoyalUr.net)

- 2 players, 7 pieces each, 4 binary dice. Roll 0..4 with weights 1,4,6,4,1 out of 16.
- Each track has 14 squares. Step 0 = start pool, 1..14 = on the board, 15 = borne off.
- Steps 1-4 and 13-14 are private. Steps 5-12 are shared: player A's step s is player B's step s.
- Rosettes at steps 4, 8, 14. Landing on one gives another roll. A piece on step 8 cannot be captured, so the opponent cannot move onto it.
- A move takes the piece on step s (0 = enter from the pool, one move for all pool pieces) to s + roll. Illegal if: roll is 0; s + roll > 15 (exact bear-off); the destination 1..14 holds your own piece; the destination is 8 and holds an enemy piece.
- Landing on an enemy piece on a shared square (except 8) sends it back to its pool.
- Roll 0, or no legal move, passes the turn. If any move exists you must move (capture is not forced).
- No extra roll for capture or bear-off. If an extra roll has no legal move the turn passes.
- First to bear off all 7 wins. Who starts is up to the caller (`ur_init(p, first)`).

## API (`ur_rules.h`)

```c
typedef struct { uint16_t mask[2]; uint8_t pool[2], done[2], turn; } UrPos;  // 12 bytes, copy by value
typedef struct { uint8_t from, to; } UrMove;      // from 0 = enter, to 15 = bear off
typedef struct { bool captured, extra_turn, borne_off; int8_t winner;
                 uint8_t from, to, player; } UrMoveResult;

void ur_init(UrPos *p, int first_player);
int  ur_gen_moves(const UrPos *p, int roll, UrMove out[UR_MAX_MOVES]);  // count, ascending from-step
UrMoveResult ur_apply(UrPos *p, UrMove m, int roll);   // switches turn unless extra_turn
void ur_pass(UrPos *p);                                // roll 0 / no legal move
int  ur_winner(const UrPos *p);                        // -1, 0 or 1
bool ur_pos_valid(const UrPos *p);                     // invariants, for tests/asserts

void ur_rng_seed(UrRng *r, uint64_t seed);             // xorshift64*, splitmix seeding
int  ur_roll(UrRng *r);                                // 0..4, four coin flips
uint32_t ur_rng_below(UrRng *r, uint32_t n);

bool ur_step_cell(int player, int step, int *row, int *col);  // false for steps 0 and 15
int  ur_cell_step(int player, int row, int col);              // -1 if not on that track
```

`mask[p]` bit s (1..14) is set when player p has a piece on step s. `UR_MAX_MOVES` is 7.
`ur_apply` does not validate: pass it a move from `ur_gen_moves`. In `UrMoveResult`, when `captured` is
true the victim sat on the shared square `to`. After a winning move `turn` has already switched
(RoyalUr's own win state has no turn; the cross-check ignores it).

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

Cell mapping: rows 0..2, cols 0..7. Player 0 uses row 2, player 1 row 0. For a player on row r:
steps 1-4 = (r,3),(r,2),(r,1),(r,0); steps 5-12 = (1,0)..(1,7); step 13 = (r,7); step 14 = (r,6).
Rosette cells are (r,0), (1,3), (r,6).

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

Static eval (from one side's view, difference of both sides): 10 per step of progress (a borne-off piece
counts 15 steps) + 20 per borne-off piece + 45 for holding step 8 + 15 per piece still in the pool,
minus expected capture loss: for each piece on a shared square (not 8), the probability that an enemy
piece 1-4 steps behind rolls the right number times (10 * step + 15). Weights are in `UrEvalW`
(defaults were tuned by self-play at a 3000-node budget: about 55% against the first guess).

| Level | Behaviour |
|---|---|
| `UR_BEGINNER` | 30% of moves picked at random; otherwise greedy 1-ply on the static eval with +-120 noise per move (12 steps of progress), so it often misses captures and leaves pieces exposed. Beats a uniformly random mover 87% of games. |
| `UR_MEDIUM` | Depth 2 (full window), +-20 noise on each root value. |
| `UR_EXPERT` | Iterative deepening, depth 1, 2, 3... until the next depth would not fit in `expert_budget` (100,000 nodes). Result of the last completed depth. |

## Running the tests

```sh
make -C host test      # unit tests, fuzz, AI checks (~35 s)
make -C host ladder    # self-play ladder, 2000 games per pairing (~5 min)
make -C host arm       # compile game/*.c with devkitARM (warnings only)
make -C host xcheck    # cross-check against royalur-python (needs tools/xcheck/.venv)
build/ladder <A> <B> <games> [seed] [a.key=val b.key=val ...]   # from host/, e.g. ladder expert medium 2000 101
```

Cross-check: `tools/xcheck/gen.c` plays random and AI-driven games and writes a transcript (legal
moves, chosen move, resulting state per roll). `tools/xcheck/check.py` replays it in `royalur`
(Finkel settings: 7 pieces, 4 binary dice, Bell path, safe rosettes, extra roll on rosettes only) and
compares legal moves and state after every roll. Setup: `python3 -m venv tools/xcheck/.venv &&
tools/xcheck/.venv/bin/pip install royalur` (`run.sh` does this if the venv is missing).
royalur 0.0.6 note: `RuleSet.paths` has no `return`, so `check.py` reads `rules._paths`.
The library always lets LIGHT (player 0) start, so transcripts always start with player 0.

## Measured numbers (Mac, Apple Silicon, clang -O2)

All from the committed code; seeds in the commands above. Ladder: sides and first mover alternate.

| Match | Games | First wins | 95% CI (Wilson) |
|---|---|---|---|
| Expert vs Medium | 2000 | Expert 68.1% (1362) | 66.0-70.1% |
| Medium vs Beginner | 2000 | Medium 98.2% (1963) | 97.5-98.7% |
| Expert vs Beginner | 2000 | Expert 99.4% (1988) | 99.0-99.7% |
| Beginner vs uniform random | 4000 | Beginner 87.2% | 86.2-88.2% |

Nodes per move (only moves with a real choice):

| Level | min | avg | max | avg depth | Mac time/move |
|---|---|---|---|---|---|
| Expert | 6253 | ~50,000 | 100,000 | 4.9 | 2.2 ms |
| Medium | 10 | 36 | 118 | 2 | 0.002 ms |
| Beginner | 0 | 0 | 0 | 0 | ~0 |

The Mac does about 23 million nodes/s, so Expert's 100k budget is ~4 ms here. On the 3DS plan for
roughly 1 s worst case at 100k nodes/s; lower `expert_budget` in `UrAiConfig` if hardware measures slower.
Compiled size with devkitARM -O2: ur_rules.o 1.6 KB, ur_ai.o 3.1 KB text, no data/bss.
