/* Royal Game of Ur, Finkel rules (as on RoyalUr.net). Portable C99, no heap, no globals. */
#ifndef UR_RULES_H
#define UR_RULES_H

#include <stdbool.h>
#include <stdint.h>

#define UR_PIECES     7    /* pieces per side */
#define UR_MAX_TRACK  16   /* longest track (Twenty Squares) */
#define UR_MAX_MOVES  7

/* Two boards, same Finkel rules. Steps: 0 = in the start pool, 1..track = on the board,
   track + 1 = borne off ("off"). */
enum { UR_BOARD_UR = 0, UR_BOARD_TWENTY = 1, UR_BOARD_COUNT };

typedef struct {
    const char *name;   /* "Ur", "Twenty Squares" */
    int      track;     /* 14 or 16 */
    int      shared_lo, shared_hi;   /* steps shared with the opponent */
    uint32_t rosettes;  /* bit s: step s is a rosette (extra roll) */
    uint32_t safe;      /* bit s: shared rosette (cannot be captured / landed on by the enemy) */
    int      cols;      /* 8 or 12 */
} UrBoardDef;

extern const UrBoardDef UR_BOARDS[UR_BOARD_COUNT];

typedef struct {
    uint32_t mask[2];   /* bit s set = this player has a piece on step s (s in 1..track) */
    uint8_t  pool[2];   /* pieces not yet entered */
    uint8_t  done[2];   /* pieces borne off */
    uint8_t  turn;      /* player to roll / move: 0 or 1 */
    uint8_t  board;     /* UR_BOARD_UR or UR_BOARD_TWENTY */
} UrPos;

typedef struct {
    uint8_t from;       /* 0 = enter a piece from the pool */
    uint8_t to;         /* 1..track, or track + 1 = bear off */
} UrMove;

typedef struct {
    bool    captured;   /* an opponent piece was sent back; it was on step `to` */
    bool    extra_turn;
    bool    borne_off;
    int8_t  winner;     /* -1 none, else 0 / 1 */
    uint8_t from, to;
    uint8_t player;     /* who moved */
} UrMoveResult;

/* Small seedable RNG (xorshift64*), used for dice and AI noise. */
typedef struct { uint64_t s; } UrRng;

void     ur_rng_seed(UrRng *r, uint64_t seed);
uint32_t ur_rng_u32(UrRng *r);
uint32_t ur_rng_below(UrRng *r, uint32_t n);   /* 0..n-1 */
int      ur_roll(UrRng *r);                    /* 0..4: four coin flips */

/* Roll probabilities, out of 16, for rolls 0..4. */
extern const uint8_t UR_ROLL_WEIGHT[5];

void ur_init(UrPos *p, int first_player, int board);
int  ur_off(const UrPos *p);                   /* track + 1: the "borne off" step */

bool ur_is_rosette(int board, int step);       /* Ur: 4, 8, 14; Twenty: 4, 8, 12, 16 */
bool ur_is_shared(int board, int step);        /* Ur: 5..12; Twenty: 5..16 */
bool ur_is_safe(int board, int step);          /* shared rosette: Ur 8; Twenty 8, 12, 16 */

/* Legal moves for p->turn with this roll, ascending from-step. Returns count (0 if roll is 0). */
int  ur_gen_moves(const UrPos *p, int roll, UrMove out[UR_MAX_MOVES]);

/* Apply a legal move for p->turn. Switches turn unless the move earns an extra roll. */
UrMoveResult ur_apply(UrPos *p, UrMove m, int roll);

/* Roll of 0, or no legal move: the turn passes. */
void ur_pass(UrPos *p);

int  ur_winner(const UrPos *p);                /* -1 if the game is still on */

/* Consistency check used by tests/fuzz: piece counts, mask range, shared squares. */
bool ur_pos_valid(const UrPos *p);

/* Board cells for the renderer. Rows 0..2, cols 0..cols-1. Player 0 uses row 2, player 1 row 0.
   Steps 1-4 = (r,3),(r,2),(r,1),(r,0); shared steps from 5 = (1,0),(1,1)...;
   Ur: step 13 = (r,7), 14 = (r,6). Twenty: shared steps run to 16 = (1,11). */
bool ur_cell_exists(int board, int row, int col);
bool ur_step_cell(int board, int player, int step, int *row, int *col);   /* false for pool / off */
int  ur_cell_step(int board, int player, int row, int col);               /* -1 if not on that track */

#endif
