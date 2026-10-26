#include "ur_rules.h"

const uint8_t UR_ROLL_WEIGHT[5] = { 1, 4, 6, 4, 1 };

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

void ur_init(UrPos *p, int first_player)
{
    p->mask[0] = p->mask[1] = 0;
    p->pool[0] = p->pool[1] = UR_PIECES;
    p->done[0] = p->done[1] = 0;
    p->turn = (uint8_t)(first_player & 1);
}

bool ur_is_rosette(int step) { return step == 4 || step == 8 || step == 14; }
bool ur_is_shared(int step)  { return step >= UR_SHARED_LO && step <= UR_SHARED_HI; }

int ur_gen_moves(const UrPos *p, int roll, UrMove out[UR_MAX_MOVES])
{
    int n = 0;
    if (roll < 1 || roll > 4)
        return 0;

    const int me = p->turn, opp = me ^ 1;
    const unsigned own = p->mask[me], theirs = p->mask[opp];

    for (int from = (p->pool[me] > 0 ? 0 : 1); from <= UR_TRACK; from++) {
        if (from > 0 && !(own & (1u << from)))
            continue;
        int to = from + roll;
        if (to > UR_OFF)
            continue;
        if (to < UR_OFF) {
            if (own & (1u << to))
                continue;
            if (to == UR_CENTRE && (theirs & (1u << to)))
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

    if (m.from == 0)
        p->pool[me]--;
    else
        p->mask[me] &= (uint16_t)~(1u << m.from);

    if (m.to == UR_OFF) {
        p->done[me]++;
        r.borne_off = true;
        if (p->done[me] == UR_PIECES)
            r.winner = (int8_t)me;
    } else {
        p->mask[me] |= (uint16_t)(1u << m.to);
        if (ur_is_shared(m.to) && (p->mask[opp] & (1u << m.to))) {
            p->mask[opp] &= (uint16_t)~(1u << m.to);
            p->pool[opp]++;
            r.captured = true;
        }
        r.extra_turn = ur_is_rosette(m.to);
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
    if (p->done[0] == UR_PIECES) return 0;
    if (p->done[1] == UR_PIECES) return 1;
    return -1;
}

static int popcount16(unsigned v)
{
    int n = 0;
    while (v) { v &= v - 1; n++; }
    return n;
}

bool ur_pos_valid(const UrPos *p)
{
    const unsigned legal = 0x7FFEu;   /* bits 1..14 */
    if (p->turn > 1)
        return false;
    for (int i = 0; i < 2; i++) {
        if (p->mask[i] & ~legal)
            return false;
        if (p->pool[i] + p->done[i] + popcount16(p->mask[i]) != UR_PIECES)
            return false;
    }
    /* shared squares: never held by both sides */
    unsigned sharedbits = 0;
    for (int s = UR_SHARED_LO; s <= UR_SHARED_HI; s++)
        sharedbits |= 1u << s;
    if (p->mask[0] & p->mask[1] & sharedbits)
        return false;
    return !(p->done[0] == UR_PIECES && p->done[1] == UR_PIECES);
}

/* ---- board cells ---- */

bool ur_step_cell(int player, int step, int *row, int *col)
{
    int r = (player == 0) ? 2 : 0;
    if (step < 1 || step > UR_TRACK)
        return false;
    if (step <= 4)       { *row = r; *col = 4 - step; }
    else if (step <= 12) { *row = 1; *col = step - 5; }
    else                 { *row = r; *col = (step == 13) ? 7 : 6; }
    return true;
}

int ur_cell_step(int player, int row, int col)
{
    int r = (player == 0) ? 2 : 0;
    if (col < 0 || col > 7 || row < 0 || row > 2)
        return -1;
    if (row == 1)
        return col + 5;
    if (row != r)
        return -1;
    if (col <= 3) return 4 - col;
    if (col == 6) return 14;
    if (col == 7) return 13;
    return -1;
}
