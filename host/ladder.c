/* Self-play ladder: ladder <A> <B> <games> [seed] [a.key=val b.key=val ...]
   or:                ladder all <games> [seed]
   Levels: beginner | medium | expert. Sides and first mover alternate. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../game/ur_rules.h"
#include "../game/ur_ai.h"

typedef struct {
    UrLevel level;
    UrAiConfig cfg;
    unsigned long long moves;       /* moves with a real choice */
    unsigned long long nodes, nmin, nmax, depth_sum;
    double secs;
} Side;

static const char *LNAME[3] = { "beginner", "medium", "expert" };

static int parse_level(const char *s)
{
    for (int i = 0; i < 3; i++) if (!strcmp(s, LNAME[i])) return i;
    fprintf(stderr, "bad level %s\n", s);
    exit(2);
}

static void set_key(Side *s, const char *kv)
{
    char key[32]; float v;
    if (sscanf(kv, "%31[^=]=%f", key, &v) != 2) { fprintf(stderr, "bad option %s\n", kv); exit(2); }
    UrAiConfig *c = &s->cfg;
    if (!strcmp(key, "step")) c->w.step = v;
    else if (!strcmp(key, "done")) c->w.done = v;
    else if (!strcmp(key, "centre")) c->w.centre = v;
    else if (!strcmp(key, "danger")) c->w.danger = v;
    else if (!strcmp(key, "loss")) c->w.loss = v;
    else if (!strcmp(key, "defer")) c->w.defer = v;
    else if (!strcmp(key, "pool")) c->w.pool = v;
    else if (!strcmp(key, "budget")) c->expert_budget = (uint32_t)v;
    else if (!strcmp(key, "mdepth")) c->medium_depth = (int)v;
    else if (!strcmp(key, "mnoise")) c->medium_noise = v;
    else if (!strcmp(key, "bnoise")) c->beginner_noise = v;
    else if (!strcmp(key, "brand")) c->beginner_random = (int)v;
    else { fprintf(stderr, "unknown key %s\n", key); exit(2); }
}

static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }

/* returns winner (0 or 1) */
static int play(Side side[2], int first, UrRng *rng)
{
    UrPos p;
    ur_init(&p, first);
    while (ur_winner(&p) < 0) {
        int roll = ur_roll(rng);
        UrMove mv[UR_MAX_MOVES];
        int n = ur_gen_moves(&p, roll, mv);
        if (n == 0) { ur_pass(&p); continue; }
        Side *s = &side[p.turn];
        UrAiStats st;
        double t0 = now();
        UrMove m = ur_ai_choose_cfg(&p, roll, s->level, rng, &s->cfg, &st);
        s->secs += now() - t0;
        if (n > 1) {
            s->moves++;
            s->nodes += st.nodes;
            if (st.nodes < s->nmin) s->nmin = st.nodes;
            if (st.nodes > s->nmax) s->nmax = st.nodes;
            s->depth_sum += (unsigned)st.depth;
        }
        ur_apply(&p, m, roll);
    }
    return ur_winner(&p);
}

static void wilson(int w, int n, double *lo, double *hi)
{
    double z = 1.96, ph = (double)w / n;
    double d = 1 + z * z / n;
    double c = ph + z * z / (2 * n);
    double e = z * sqrt(ph * (1 - ph) / n + z * z / (4.0 * n * n));
    *lo = (c - e) / d; *hi = (c + e) / d;
}

static void print_side(const char *tag, const Side *s)
{
    if (!s->moves) { printf("  %s %-8s: no real choices\n", tag, LNAME[s->level]); return; }
    printf("  %s %-8s: %llu decisions, nodes/move min %llu avg %.0f max %llu, avg depth %.2f, %.3f ms/move (Mac)\n",
           tag, LNAME[s->level], s->moves, s->nmin, (double)s->nodes / s->moves, s->nmax,
           (double)s->depth_sum / s->moves, 1000.0 * s->secs / s->moves);
}

static void match(int la, int lb, int games, uint64_t seed, char **opts, int nopts)
{
    Side side[2];
    UrRng rng;
    ur_rng_seed(&rng, seed);
    for (int i = 0; i < 2; i++) {
        memset(&side[i], 0, sizeof side[i]);
        side[i].level = (UrLevel)(i ? lb : la);
        side[i].cfg = UR_AI_DEFAULT;
        side[i].nmin = ~0ull;
    }
    for (int i = 0; i < nopts; i++) {
        if (!strncmp(opts[i], "a.", 2)) set_key(&side[0], opts[i] + 2);
        else if (!strncmp(opts[i], "b.", 2)) set_key(&side[1], opts[i] + 2);
        else { fprintf(stderr, "option must start with a. or b.\n"); exit(2); }
    }

    /* Side structs are indexed by seat; A sits in seat (g&1), the first mover alternates too. */
    Side A = side[0], B = side[1];
    A.nmin = B.nmin = ~0ull;
    int a_wins = 0;
    for (int g = 0; g < games; g++) {
        int a_seat = g & 1, first = (g >> 1) & 1;
        Side seats[2];
        seats[a_seat] = A;
        seats[a_seat ^ 1] = B;
        int w = play(seats, first, &rng);
        A = seats[a_seat];
        B = seats[a_seat ^ 1];
        if (w == a_seat) a_wins++;
    }
    double lo, hi;
    wilson(a_wins, games, &lo, &hi);
    printf("%s vs %s, %d games: %s wins %d (%.1f%%, 95%% CI %.1f-%.1f%%), %s wins %d\n",
           LNAME[la], LNAME[lb], games, LNAME[la], a_wins, 100.0 * a_wins / games, 100 * lo, 100 * hi,
           LNAME[lb], games - a_wins);
    print_side("A", &A);
    print_side("B", &B);
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "all")) {
        int games = atoi(argv[2]);
        uint64_t seed = argc > 3 ? strtoull(argv[3], 0, 10) : 1;
        match(UR_EXPERT, UR_MEDIUM, games, seed, NULL, 0);
        match(UR_MEDIUM, UR_BEGINNER, games, seed + 1, NULL, 0);
        match(UR_EXPERT, UR_BEGINNER, games, seed + 2, NULL, 0);
        return 0;
    }
    if (argc < 4) {
        fprintf(stderr, "usage: ladder <A> <B> <games> [seed] [a.key=val ...] | ladder all <games> [seed]\n");
        return 2;
    }
    int next = 4;
    uint64_t seed = 1;
    if (argc > 4 && !strchr(argv[4], '=')) { seed = strtoull(argv[4], 0, 10); next = 5; }
    match(parse_level(argv[1]), parse_level(argv[2]), atoi(argv[3]), seed, argv + next, argc - next);
    return 0;
}
