/* Royal Game of Ur, Finkel rules (as on RoyalUr.net). Portable C99, no heap, no globals. */
#ifndef UR_RULES_H
#define UR_RULES_H

#include <stdbool.h>
#include <stdint.h>

#define UR_PIECES     7    /* pieces per side */
#define UR_TRACK      14   /* squares on a player's track; steps 1..14 */
#define UR_OFF        15   /* step value for "borne off" */
#define UR_MAX_MOVES  7
#define UR_SHARED_LO  5    /* steps 5..12 are shared with the opponent */
#define UR_SHARED_HI  12
#define UR_CENTRE     8

/* Steps: 0 = in the start pool, 1..14 = on the board, 15 = borne off. */
typedef struct {
    uint16_t mask[2];   /* bit s set = this player has a piece on step s (s in 1..14) */
    uint8_t  pool[2];   /* pieces not yet entered */
    uint8_t  done[2];   /* pieces borne off */
    uint8_t  turn;      /* player to roll / move: 0 or 1 */
} UrPos;

typedef struct {
    uint8_t from;       /* 0 = enter a piece from the pool */
    uint8_t to;         /* 1..14, or 15 = bear off */
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

void ur_init(UrPos *p, int first_player);

bool ur_is_rosette(int step);                  /* 4, 8, 14 */
bool ur_is_shared(int step);                   /* 5..12 */

/* Legal moves for p->turn with this roll, ascending from-step. Returns count (0 if roll is 0). */
int  ur_gen_moves(const UrPos *p, int roll, UrMove out[UR_MAX_MOVES]);

/* Apply a legal move for p->turn. Switches turn unless the move earns an extra roll. */
UrMoveResult ur_apply(UrPos *p, UrMove m, int roll);

/* Roll of 0, or no legal move: the turn passes. */
void ur_pass(UrPos *p);

int  ur_winner(const UrPos *p);                /* -1 if the game is still on */

/* Consistency check used by tests/fuzz: piece counts, mask range, shared squares. */
bool ur_pos_valid(const UrPos *p);

/* Board cells for the renderer. Rows 0..2, cols 0..7. Player 0 uses row 2, player 1 row 0. */
bool ur_step_cell(int player, int step, int *row, int *col);   /* false for steps 0 and 15 */
int  ur_cell_step(int player, int row, int col);               /* -1 if not on that player's track */

#endif
