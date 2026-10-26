#include "ur_ai.h"

#define WIN   2000.0f           /* static evals are clamped to +-EVAL_MAX, wins sit above that */
#define EVAL_MAX 1900.0f
#define BOUND (WIN + 20.0f)     /* above any value the search can return */
#define INF   1.0e9f
#define EPS   0.01f

const UrAiConfig UR_AI_DEFAULT = {
    { 10.0f, 20.0f, 45.0f, 1.0f, 15.0f, 0.0f, 15.0f },
    100000u, 12,
    2, 20.0f,
    120.0f, 30
};

/* P(roll = d) for d = 1..4, as fractions of 16 */
static const float PROB[5] = { 1.0f / 16, 4.0f / 16, 6.0f / 16, 4.0f / 16, 1.0f / 16 };

/* ---- evaluation ---- */

/* Progress is weighted per step relative to the board's track (14 / track), so a whole trip
   round the board is worth the same on both boards. Exactly 1.0 on Ur. */
static float step_weight(const UrPos *p, const UrEvalW *w)
{
    return w->step * (14.0f / (float)UR_BOARDS[p->board].track);
}

static float side_score(const UrPos *p, int x, const UrEvalW *w)
{
    const UrBoardDef *b = &UR_BOARDS[p->board];
    const float sw = step_weight(p, w);
    float s = 0;
    uint32_t m = p->mask[x];
    for (int st = 1; st <= b->track; st++) {
        if (m & (1u << st)) {
            s += st * sw;
            if ((b->safe >> st) & 1u)
                s += w->centre;          /* holding a safe square */
        }
    }
    s += p->done[x] * ((b->track + 1) * sw + w->done);
    s -= p->pool[x] * w->pool;
    return s;
}

/* Expected cost of capture threats against x's pieces on shared, non-safe squares. */
static float danger(const UrPos *p, int x, const UrEvalW *w)
{
    const UrBoardDef *b = &UR_BOARDS[p->board];
    const float sw = step_weight(p, w);
    const uint32_t mine = p->mask[x], theirs = p->mask[x ^ 1];
    float total = 0;
    for (int s = b->shared_lo; s <= b->shared_hi; s++) {
        if ((b->safe >> s & 1u) || !(mine & (1u << s)))
            continue;
        float prob = 0;
        for (int d = 1; d <= 4; d++) {
            int t = s - d;
            if (t >= 1 && (theirs & (1u << t)))
                prob += PROB[d];
        }
        total += prob * (s * sw + w->loss);
    }
    return total * (p->turn == x ? w->defer : 1.0f);
}

float ur_eval(const UrPos *p, int me, const UrEvalW *w)
{
    int opp = me ^ 1;
    float v = side_score(p, me, w) - side_score(p, opp, w)
            - w->danger * (danger(p, me, w) - danger(p, opp, w));
    return v > EVAL_MAX ? EVAL_MAX : (v < -EVAL_MAX ? -EVAL_MAX : v);
}

/* ---- search ---- */

typedef struct {
    const UrEvalW *w;
    int      me;
    bool     prune;
    bool     aborted;
    uint32_t nodes, budget;
} Search;

static float chance(Search *s, const UrPos *p, int depth, float alpha, float beta);

static int move_key(const UrPos *p, UrMove m)
{
    int k = m.to;
    if (m.to == ur_off(p)) k += 20;
    else if (ur_is_shared(p->board, m.to) && !ur_is_safe(p->board, m.to) && (p->mask[p->turn ^ 1] & (1u << m.to))) k += 40;
    if (ur_is_rosette(p->board, m.to)) k += 30;
    return k;
}

static float after_roll(Search *s, const UrPos *p, int roll, int depth, float alpha, float beta)
{
    UrMove mv[UR_MAX_MOVES];
    int n = ur_gen_moves(p, roll, mv);

    if (n == 0) {
        UrPos q = *p;
        ur_pass(&q);
        return chance(s, &q, depth - 1, alpha, beta);
    }

    /* best-first ordering helps the pruning */
    int key[UR_MAX_MOVES];
    for (int i = 0; i < n; i++) {
        key[i] = move_key(p, mv[i]);
        for (int j = i; j > 0 && key[j] > key[j - 1]; j--) {
            int tk = key[j]; key[j] = key[j - 1]; key[j - 1] = tk;
            UrMove tm = mv[j]; mv[j] = mv[j - 1]; mv[j - 1] = tm;
        }
    }

    const bool maxing = (p->turn == s->me);
    float best = maxing ? -INF : INF;
    for (int i = 0; i < n; i++) {
        UrPos q = *p;
        UrMoveResult r = ur_apply(&q, mv[i], roll);
        float v;
        if (r.winner >= 0)
            v = (r.winner == s->me) ? (WIN + depth) : -(WIN + depth);
        else
            v = chance(s, &q, depth - 1, alpha, beta);
        if (s->aborted)
            return 0;
        if (maxing) {
            if (v > best) best = v;
            if (best > alpha) alpha = best;
        } else {
            if (v < best) best = v;
            if (best < beta) beta = best;
        }
        if (s->prune && alpha >= beta)
            break;
    }
    return best;
}

/* Value of a position just before p->turn rolls. */
static float chance(Search *s, const UrPos *p, int depth, float alpha, float beta)
{
    if (s->nodes >= s->budget) {
        s->aborted = true;
        return 0;
    }
    s->nodes++;
    if (depth <= 0)
        return ur_eval(p, s->me, s->w);

    float sum = 0;                       /* sum of weight * value over outcomes so far */
    int rem = 16;                        /* weight of outcomes not yet searched */
    for (int r = 0; r <= 4; r++) {
        int wt = UR_ROLL_WEIGHT[r];
        rem -= wt;
        float a = -INF, b = INF;
        if (s->prune) {
            a = (16.0f * alpha - sum - rem * BOUND) / wt - EPS;
            b = (16.0f * beta  - sum + rem * BOUND) / wt + EPS;
        }
        float v = after_roll(s, p, r, depth, a, b);
        if (s->aborted)
            return 0;
        sum += wt * v;
        if (s->prune) {
            if (sum + rem * BOUND <= 16.0f * alpha) return (sum + rem * BOUND) / 16.0f;
            if (sum - rem * BOUND >= 16.0f * beta)  return (sum - rem * BOUND) / 16.0f;
        }
    }
    return sum / 16.0f;
}

float ur_search_value(const UrPos *p, int me, int depth, bool prune, const UrEvalW *w, uint32_t *nodes)
{
    Search s;
    s.w = w; s.me = me; s.prune = prune; s.aborted = false;
    s.nodes = 0; s.budget = 0xFFFFFFFFu;
    float v = chance(&s, p, depth, -INF, INF);
    if (nodes) *nodes = s.nodes;
    return v;
}

/* Values of each root move at a given depth (p->turn is the mover, roll known).
   With full=false, moves that can't beat the best so far return an upper bound only.
   Returns index of best move, or -1 if the budget ran out. */
static int root_search(Search *s, const UrPos *p, int roll, const UrMove *mv, int n,
                       int depth, bool full, float *vals)
{
    float best = -INF;
    int bi = 0;
    for (int i = 0; i < n; i++) {
        UrPos q = *p;
        UrMoveResult r = ur_apply(&q, mv[i], roll);
        float v;
        if (r.winner >= 0)
            v = WIN + depth;
        else
            v = chance(s, &q, depth - 1, (full || !s->prune) ? -INF : best - EPS, INF);
        if (s->aborted)
            return -1;
        vals[i] = v;
        if (v > best) { best = v; bi = i; }
    }
    return bi;
}

/* ---- level policies ---- */

static UrMove random_move(const UrMove *mv, int n, UrRng *rng)
{
    return mv[ur_rng_below(rng, (uint32_t)n)];
}

static float noise(UrRng *rng, float amp)
{
    return ((float)ur_rng_below(rng, 2001) / 1000.0f - 1.0f) * amp;
}

static UrMove choose_beginner(const UrPos *p, int roll, const UrMove *mv, int n,
                              UrRng *rng, const UrAiConfig *cfg)
{
    if ((int)ur_rng_below(rng, 100) < cfg->beginner_random)
        return random_move(mv, n, rng);

    int me = p->turn, bi = 0;
    float best = -INF;
    for (int i = 0; i < n; i++) {
        UrPos q = *p;
        UrMoveResult r = ur_apply(&q, mv[i], roll);
        float v = (r.winner >= 0) ? WIN : ur_eval(&q, me, &cfg->w);
        v += noise(rng, cfg->beginner_noise);
        if (v > best) { best = v; bi = i; }
    }
    return mv[bi];
}

UrMove ur_ai_choose_cfg(const UrPos *p, int roll, UrLevel level, UrRng *rng,
                        const UrAiConfig *cfg, UrAiStats *stats)
{
    UrMove mv[UR_MAX_MOVES];
    int n = ur_gen_moves(p, roll, mv);
    Search s;
    float vals[UR_MAX_MOVES];

    if (stats) { stats->nodes = 0; stats->depth = 0; }
    if (n <= 1)
        return mv[0];

    s.w = &cfg->w; s.me = p->turn; s.prune = true; s.aborted = false;
    s.nodes = 0; s.budget = 0xFFFFFFFFu;

    if (level == UR_BEGINNER)
        return choose_beginner(p, roll, mv, n, rng, cfg);

    if (level == UR_MEDIUM) {
        root_search(&s, p, roll, mv, n, cfg->medium_depth, true, vals);
        int bi = 0;
        float best = -INF;
        for (int i = 0; i < n; i++) {
            float v = vals[i] + noise(rng, cfg->medium_noise);
            if (v > best) { best = v; bi = i; }
        }
        if (stats) { stats->nodes = s.nodes; stats->depth = cfg->medium_depth; }
        return mv[bi];
    }

    /* Expert: iterative deepening under a node budget; the last completed depth wins. */
    s.budget = cfg->expert_budget;
    UrMove order[UR_MAX_MOVES];
    for (int i = 0; i < n; i++) order[i] = mv[i];
    UrMove pick = mv[0];
    int done_depth = 0;
    uint32_t prev_cost = 0, last_cost = 0;

    for (int d = 1; d <= cfg->expert_max_depth; d++) {
        /* skip an iteration that will obviously not fit */
        if (last_cost > 0) {
            float ratio = prev_cost ? (float)last_cost / (float)prev_cost : 4.0f;
            if (ratio < 3.0f) ratio = 3.0f;
            if ((float)s.nodes + (float)last_cost * ratio > (float)cfg->expert_budget)
                break;
        }
        uint32_t before = s.nodes;
        int bi = root_search(&s, p, roll, order, n, d, false, vals);
        if (bi < 0)
            break;
        pick = order[bi];
        done_depth = d;
        prev_cost = last_cost;
        last_cost = s.nodes - before;
        /* best move first next time */
        UrMove t = order[0]; order[0] = order[bi]; order[bi] = t;
    }
    if (stats) { stats->nodes = s.nodes; stats->depth = done_depth; }
    return pick;
}

UrMove ur_ai_choose(const UrPos *p, int roll, UrLevel level, UrRng *rng, UrAiStats *stats)
{
    return ur_ai_choose_cfg(p, roll, level, rng, &UR_AI_DEFAULT, stats);
}
