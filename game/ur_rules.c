#include "ur_rules.h"

const uint8_t UR_ROLL_WEIGHT[5] = { 1, 4, 6, 4, 1 };

const UrBoardDef UR_BOARDS[UR_BOARD_COUNT] = {
    { "Ur",             14,  5, 12, (1u << 4) | (1u << 8) | (1u << 14),
                                    1u << 8, 8, 7 },
    { "Twenty Squares", 16,  5, 16, (1u << 4) | (1u << 8) | (1u << 12) | (1u << 16),
                                    (1u << 8) | (1u << 12) | (1u << 16), 12, 5 }
};

static const UrBoardDef *bd(int board)
{
    return &UR_BOARDS[board == UR_BOARD_TWENTY ? UR_BOARD_TWENTY : UR_BOARD_UR];
}

/* ---- RNG ---- */

void ur_rng_seed(UrRng *r, uint64_t seed)
{
    /* splitmix64 so that small seeds give well-mixed state */
    uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    r->s = z ? z : 0x2545F4914F6CDD1DULL;
}

uint32_t ur_rng_u32(UrRng *r)
{
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

uint32_t ur_rng_below(UrRng *r, uint32_t n)
{
    return (uint32_t)(((uint64_t)ur_rng_u32(r) * n) >> 32);
}

int ur_roll(UrRng *r)
{
    uint32_t bits = ur_rng_u32(r) >> 28;   /* top 4 bits: four fair coins */
    int n = 0;
    for (int i = 0; i < 4; i++) {
        n += (int)(bits & 1u);
        bits >>= 1;
    }
    return n;
}

/* ---- position ---- */

void ur_init(UrPos *p, int first_player, int board)
{
    p->mask[0] = p->mask[1] = 0;
    p->board = (uint8_t)(board == UR_BOARD_TWENTY ? UR_BOARD_TWENTY : UR_BOARD_UR);
    p->pool[0] = p->pool[1] = (uint8_t)bd(p->board)->pieces;
    p->done[0] = p->done[1] = 0;
    p->turn = (uint8_t)(first_player & 1);
}

int ur_pieces(int board) { return bd(board)->pieces; }

int ur_off(const UrPos *p) { return bd(p->board)->track + 1; }

bool ur_is_rosette(int board, int step)
{
    return step >= 1 && step <= UR_MAX_TRACK && ((bd(board)->rosettes >> step) & 1u);
}

bool ur_is_shared(int board, int step)
{
    const UrBoardDef *b = bd(board);
    return step >= b->shared_lo && step <= b->shared_hi;
}

bool ur_is_safe(int board, int step)
{
    return step >= 1 && step <= UR_MAX_TRACK && ((bd(board)->safe >> step) & 1u);
}

int ur_gen_moves(const UrPos *p, int roll, UrMove out[UR_MAX_MOVES])
{
    int n = 0;
    if (roll < 1 || roll > 4)
        return 0;

    const UrBoardDef *b = bd(p->board);
    const int me = p->turn, opp = me ^ 1, off = b->track + 1;
    const uint32_t own = p->mask[me], theirs = p->mask[opp];

    for (int from = (p->pool[me] > 0 ? 0 : 1); from <= b->track; from++) {
        if (from > 0 && !(own & (1u << from)))
            continue;
        int to = from + roll;
        if (to > off)
            continue;
        if (to < off) {
            if (own & (1u << to))
                continue;
            if ((b->safe >> to & 1u) && (theirs & (1u << to)))
                continue;
        }
        out[n].from = (uint8_t)from;
        out[n].to = (uint8_t)to;
        n++;
    }
    return n;
}

UrMoveResult ur_apply(UrPos *p, UrMove m, int roll)
{
    UrMoveResult r;
    const int me = p->turn, opp = me ^ 1;
    (void)roll;   /* m.to already encodes it; kept in the signature for symmetry */

    r.captured = false;
    r.extra_turn = false;
    r.borne_off = false;
    r.winner = -1;
    r.from = m.from;
    r.to = m.to;
    r.player = (uint8_t)me;

    const UrBoardDef *b = bd(p->board);
    if (m.from == 0)
        p->pool[me]--;
    else
        p->mask[me] &= ~(1u << m.from);

    if (m.to == b->track + 1) {
        p->done[me]++;
        r.borne_off = true;
        if (p->done[me] == b->pieces)
            r.winner = (int8_t)me;
    } else {
        p->mask[me] |= 1u << m.to;
        if (ur_is_shared(p->board, m.to) && (p->mask[opp] & (1u << m.to))) {
            p->mask[opp] &= ~(1u << m.to);
            p->pool[opp]++;
            r.captured = true;
        }
        r.extra_turn = ur_is_rosette(p->board, m.to);
    }

    if (!r.extra_turn)
        p->turn = (uint8_t)opp;
    return r;
}

void ur_pass(UrPos *p)
{
    p->turn ^= 1;
}

int ur_winner(const UrPos *p)
{
    const int n = bd(p->board)->pieces;
    if (p->done[0] == n) return 0;
    if (p->done[1] == n) return 1;
    return -1;
}

static int popcount32(uint32_t v)
{
    int n = 0;
    while (v) { v &= v - 1; n++; }
    return n;
}

bool ur_pos_valid(const UrPos *p)
{
    if (p->turn > 1 || p->board >= UR_BOARD_COUNT)
        return false;
    const UrBoardDef *b = bd(p->board);
    const uint32_t legal = ((1u << (b->track + 1)) - 1u) & ~1u;   /* bits 1..track */
    for (int i = 0; i < 2; i++) {
        if (p->mask[i] & ~legal)
            return false;
        if (p->pool[i] + p->done[i] + popcount32(p->mask[i]) != b->pieces)
            return false;
    }
    /* shared squares: never held by both sides */
    uint32_t sharedbits = 0;
    for (int s = b->shared_lo; s <= b->shared_hi; s++)
        sharedbits |= 1u << s;
    if (p->mask[0] & p->mask[1] & sharedbits)
        return false;
    return !(p->done[0] == b->pieces && p->done[1] == b->pieces);
}

/* ---- board cells ---- */

bool ur_cell_exists(int board, int row, int col)
{
    const UrBoardDef *b = bd(board);
    if (row < 0 || row > 2 || col < 0 || col >= b->cols)
        return false;
    if (row == 1)
        return true;
    if (col <= 3)
        return true;
    /* Ur only: the outer rows also have cols 6 and 7 */
    return b->track == 14 && col >= 6;
}

bool ur_step_cell(int board, int player, int step, int *row, int *col)
{
    const UrBoardDef *b = bd(board);
    int r = (player == 0) ? 2 : 0;
    if (step < 1 || step > b->track)
        return false;
    if (step <= 4)                *row = r, *col = 4 - step;
    else if (step <= 12 || b->track == 16)
                                  *row = 1, *col = step - 5;
    else                          *row = r, *col = (step == 13) ? 7 : 6;
    return true;
}

int ur_cell_step(int board, int player, int row, int col)
{
    const UrBoardDef *b = bd(board);
    int r = (player == 0) ? 2 : 0;
    if (!ur_cell_exists(board, row, col))
        return -1;
    if (row == 1)
        return col + 5;
    if (row != r)
        return -1;
    if (col <= 3) return 4 - col;
    if (b->track == 14) {
        if (col == 6) return 14;
        if (col == 7) return 13;
    }
    return -1;
}
