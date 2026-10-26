/* Unit tests, fuzz and AI sanity checks for game/ur_rules.c and game/ur_ai.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../game/ur_rules.h"
#include "../game/ur_ai.h"

static int g_checks = 0, g_fail = 0;
static int g_board = UR_BOARD_UR;   /* board used by mk() and the board-agnostic tests */
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* Build a position: steps of player 0's and 1's pieces on the board, done counts, turn. */
static UrPos mk(int turn, const int *a, int na, int done_a, const int *b, int nb, int done_b)
{
    UrPos p;
    ur_init(&p, turn, g_board);
    for (int i = 0; i < na; i++) p.mask[0] |= 1u << a[i];
    for (int i = 0; i < nb; i++) p.mask[1] |= 1u << b[i];
    p.done[0] = (uint8_t)done_a; p.done[1] = (uint8_t)done_b;
    p.pool[0] = (uint8_t)(UR_PIECES - na - done_a);
    p.pool[1] = (uint8_t)(UR_PIECES - nb - done_b);
    return p;
}

static bool has_move(const UrPos *p, int roll, int from, int to)
{
    UrMove mv[UR_MAX_MOVES];
    int n = ur_gen_moves(p, roll, mv);
    for (int i = 0; i < n; i++)
        if (mv[i].from == from && mv[i].to == to) return true;
    return false;
}

static int count_moves(const UrPos *p, int roll)
{
    UrMove mv[UR_MAX_MOVES];
    return ur_gen_moves(p, roll, mv);
}

static void test_init_and_dice(void)
{
    UrPos p;
    ur_init(&p, 1, g_board);
    CHECK(p.turn == 1 && p.pool[0] == 7 && p.pool[1] == 7 && p.mask[0] == 0 && p.mask[1] == 0);
    CHECK(ur_pos_valid(&p));
    CHECK(count_moves(&p, 0) == 0);
    for (int r = 1; r <= 4; r++) {
        UrMove mv[UR_MAX_MOVES];
        CHECK(ur_gen_moves(&p, r, mv) == 1 && mv[0].from == 0 && mv[0].to == r);
    }

    UrRng rng;
    ur_rng_seed(&rng, 12345);
    long cnt[5] = {0}, N = 1600000;
    for (long i = 0; i < N; i++) {
        int r = ur_roll(&rng);
        CHECK(r >= 0 && r <= 4);
        cnt[r]++;
    }
    for (int r = 0; r < 5; r++) {
        double exp = (double)N * UR_ROLL_WEIGHT[r] / 16.0;
        CHECK(fabs(cnt[r] - exp) < 5.0 * sqrt(exp));   /* ~5 sigma */
    }
    printf("  roll histogram over %ld: %ld %ld %ld %ld %ld\n", N, cnt[0], cnt[1], cnt[2], cnt[3], cnt[4]);
}

static void test_rosettes(void)
{
    /* entering on 4 and moving onto 8 and 14 each give an extra roll */
    int a1[] = {2};
    UrPos p = mk(0, a1, 1, 0, NULL, 0, 0);
    UrMoveResult r = ur_apply(&p, (UrMove){0, 4}, 4);
    CHECK(r.extra_turn && p.turn == 0 && (p.mask[0] & (1u << 4)));

    int a2[] = {6};
    p = mk(0, a2, 1, 0, NULL, 0, 0);
    r = ur_apply(&p, (UrMove){6, 8}, 2);
    CHECK(r.extra_turn && p.turn == 0 && !r.captured);

    int a3[] = {12};
    p = mk(0, a3, 1, 0, NULL, 0, 0);
    r = ur_apply(&p, (UrMove){12, 14}, 2);
    CHECK(r.extra_turn && p.turn == 0);

    /* non-rosette move switches turn */
    int a4[] = {6};
    p = mk(0, a4, 1, 0, NULL, 0, 0);
    r = ur_apply(&p, (UrMove){6, 7}, 1);
    CHECK(!r.extra_turn && p.turn == 1);

    /* no extra roll for a capture or a bear-off */
    int a5[] = {5}, b5[] = {7};
    p = mk(0, a5, 1, 0, b5, 1, 0);
    r = ur_apply(&p, (UrMove){5, 7}, 2);
    CHECK(r.captured && !r.extra_turn && p.turn == 1);
    int a6[] = {14};
    p = mk(0, a6, 1, 0, NULL, 0, 0);
    r = ur_apply(&p, (UrMove){14, 15}, 1);
    CHECK(r.borne_off && !r.extra_turn && p.turn == 1);
}

static void test_central_rosette(void)
{
    /* opponent on 8 cannot be captured nor moved onto */
    int a[] = {6, 7}, b[] = {8};
    UrPos p = mk(0, a, 2, 0, b, 1, 0);
    CHECK(!has_move(&p, 2, 6, 8));
    CHECK(has_move(&p, 1, 7, 8) == false);
    CHECK(has_move(&p, 1, 6, 7) == false);      /* own piece on 7 */
    CHECK(has_move(&p, 3, 6, 9));
    /* from the pool with a roll of 4 onto private 4: fine */
    CHECK(has_move(&p, 4, 0, 4));
    /* same from player 1's side */
    int a2[] = {8}, b2[] = {5};
    p = mk(1, a2, 1, 0, b2, 1, 0);
    CHECK(!has_move(&p, 3, 5, 8));
    /* once the central rosette is free it is allowed */
    int b3[] = {5};
    p = mk(1, NULL, 0, 0, b3, 1, 0);
    CHECK(has_move(&p, 3, 5, 8));
}

static void test_capture(void)
{
    for (int s = 5; s <= 12; s++) {
        if (s == 8) continue;
        int a[] = {s - 1}, b[] = {s};
        UrPos p = mk(0, a, 1, 0, b, 1, 0);
        CHECK(has_move(&p, 1, s - 1, s));
        UrMoveResult r = ur_apply(&p, (UrMove){(uint8_t)(s - 1), (uint8_t)s}, 1);
        CHECK(r.captured && p.pool[1] == 7 && p.mask[1] == 0 && (p.mask[0] & (1u << s)));
        CHECK(ur_pos_valid(&p));
    }
    /* capture works for player 1 too */
    int a[] = {9}, b[] = {6};
    UrPos p = mk(1, a, 1, 0, b, 1, 0);
    UrMoveResult r = ur_apply(&p, (UrMove){6, 9}, 3);
    CHECK(r.captured && p.pool[0] == 7 && r.player == 1);

    /* same step number on a private square never captures: steps 1-4, 13, 14 */
    int privs[] = {1, 2, 3, 4, 13, 14};
    for (int i = 0; i < 6; i++) {
        int s = privs[i];
        int a2[] = {s - 1 >= 1 ? s - 1 : 1}, b2[] = {s};
        if (s == 1) { a2[0] = 0; }
        UrPos q;
        if (s == 1) {
            int none[] = {0};
            q = mk(0, none, 0, 0, b2, 1, 0);
            UrMoveResult rr = ur_apply(&q, (UrMove){0, 1}, 1);
            CHECK(!rr.captured && (q.mask[1] & (1u << 1)) && (q.mask[0] & (1u << 1)));
        } else {
            q = mk(0, a2, 1, 0, b2, 1, 0);
            if (s == 14) q.mask[0] = 1u << 13;
            UrMoveResult rr = ur_apply(&q, (UrMove){(uint8_t)(s - 1), (uint8_t)s}, 1);
            CHECK(!rr.captured && (q.mask[1] & (1u << s)) && (q.mask[0] & (1u << s)));
        }
        CHECK(ur_pos_valid(&q));
    }
}

static void test_bearoff_and_blocking(void)
{
    int a[] = {14, 13, 12, 11};
    UrPos p = mk(0, a, 4, 0, NULL, 0, 0);
    CHECK(has_move(&p, 1, 14, 15));
    CHECK(!has_move(&p, 2, 14, 15));       /* overshoot */
    CHECK(has_move(&p, 2, 13, 15));
    CHECK(has_move(&p, 3, 12, 15));
    CHECK(has_move(&p, 4, 11, 15));
    CHECK(!has_move(&p, 3, 13, 16));
    CHECK(!has_move(&p, 4, 12, 16));
    CHECK(!has_move(&p, 1, 13, 14));       /* own piece on 14 */
    CHECK(!has_move(&p, 1, 12, 13));       /* own piece on 13 */
    CHECK(has_move(&p, 1, 11, 12) == false);
    /* from 14 only roll 1 is legal: rolls 2..4 from 14 have no moves for that piece */
    int c[] = {14};
    p = mk(0, c, 1, 0, NULL, 0, 0);
    CHECK(!has_move(&p, 2, 14, 16) && count_moves(&p, 2) == 1);   /* pool piece enters on 2 */
    /* with an empty pool and a lone piece on 14: 2,3,4 have no move at all */
    int d[] = {14};
    p = mk(0, d, 1, 6, NULL, 0, 0);
    p.pool[0] = 0;
    CHECK(count_moves(&p, 2) == 0 && count_moves(&p, 3) == 0 && count_moves(&p, 4) == 0);
    CHECK(count_moves(&p, 1) == 1);
    /* own piece on the entry square blocks entering */
    int e[] = {3};
    p = mk(0, e, 1, 0, NULL, 0, 0);
    CHECK(!has_move(&p, 3, 0, 3));
    CHECK(has_move(&p, 2, 0, 2));
    /* moves are ascending by from-step, pool entry first */
    int f[] = {2, 5, 9};
    p = mk(0, f, 3, 0, NULL, 0, 0);
    UrMove mv[UR_MAX_MOVES];
    int n = ur_gen_moves(&p, 1, mv);
    CHECK(n == 4 && mv[0].from == 0 && mv[1].from == 2 && mv[2].from == 5 && mv[3].from == 9);
    /* 7 distinct moves is the max: 6 on board + pool */
    int g[] = {1, 2, 5, 6, 9, 10};
    p = mk(0, g, 6, 0, NULL, 0, 0);
    CHECK(count_moves(&p, 1) <= UR_MAX_MOVES);
}

static void test_pass_and_extra_roll(void)
{
    UrPos p;
    ur_init(&p, 0, g_board);
    CHECK(count_moves(&p, 0) == 0);
    ur_pass(&p);
    CHECK(p.turn == 1);
    ur_pass(&p);
    CHECK(p.turn == 0);

    /* extra roll that has no legal move: caller passes, turn goes to opponent */
    int a[] = {4};
    p = mk(0, a, 1, 6, NULL, 0, 0);
    p.pool[0] = 0;
    /* piece on 4 -> roll... move to 8 via 4 */
    UrMoveResult r = ur_apply(&p, (UrMove){4, 8}, 4);
    CHECK(r.extra_turn && p.turn == 0);
    CHECK(count_moves(&p, 4) == 1);       /* 8 -> 12 */
}

static void test_extra_roll_dead(void)
{
    /* piece lands on 14 (last piece, pool empty): rolls 2..4 have no move => pass */
    int a[] = {12};
    UrPos p = mk(0, a, 1, 6, NULL, 0, 0);
    UrMoveResult r = ur_apply(&p, (UrMove){12, 14}, 2);
    CHECK(r.extra_turn && p.turn == 0);
    CHECK(count_moves(&p, 2) == 0 && count_moves(&p, 3) == 0 && count_moves(&p, 4) == 0);
    ur_pass(&p);
    CHECK(p.turn == 1);
    CHECK(count_moves(&p, 1) >= 1);
}

static void test_win(void)
{
    int a[] = {13};
    UrPos p = mk(0, a, 1, 6, NULL, 0, 0);
    CHECK(ur_winner(&p) == -1);
    UrMoveResult r = ur_apply(&p, (UrMove){13, 15}, 2);
    CHECK(r.winner == 0 && r.borne_off && ur_winner(&p) == 0 && p.done[0] == 7);
    int b[] = {14};
    p = mk(1, NULL, 0, 0, b, 1, 6);
    r = ur_apply(&p, (UrMove){14, 15}, 1);
    CHECK(r.winner == 1 && ur_winner(&p) == 1);
    /* a bear-off that isn't the last piece does not win */
    int c[] = {14, 10};
    p = mk(0, c, 2, 5, NULL, 0, 0);
    r = ur_apply(&p, (UrMove){14, 15}, 1);
    CHECK(r.winner == -1 && p.done[0] == 6);
}

static void test_cells(void)
{
    for (int pl = 0; pl < 2; pl++) {
        int seen[3][8];
        memset(seen, 0, sizeof seen);
        int row, col;
        CHECK(!ur_step_cell(0, pl, 0, &row, &col));
        CHECK(!ur_step_cell(0, pl, 15, &row, &col));
        for (int s = 1; s <= 14; s++) {
            CHECK(ur_step_cell(0, pl, s, &row, &col));
            CHECK(row >= 0 && row <= 2 && col >= 0 && col <= 7);
            CHECK(!(row != 1 && (col == 4 || col == 5)));     /* those cells don't exist */
            CHECK(ur_cell_step(0, pl, row, col) == s);
            seen[row][col]++;
        }
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 8; c++) CHECK(seen[r][c] <= 1);
        /* cells that aren't on this track */
        int other = (pl == 0) ? 0 : 2;
        for (int c = 0; c < 8; c++) CHECK(ur_cell_step(0, pl, other, c) == -1);
        CHECK(ur_cell_step(0, pl, 1, -1) == -1 && ur_cell_step(0, pl, 3, 0) == -1);
        /* rosette cells: (r,0), (1,3), (r,6) */
        int r = (pl == 0) ? 2 : 0;
        CHECK(ur_step_cell(0, pl, 4, &row, &col) && row == r && col == 0);
        CHECK(ur_step_cell(0, pl, 8, &row, &col) && row == 1 && col == 3);
        CHECK(ur_step_cell(0, pl, 14, &row, &col) && row == r && col == 6);
        CHECK(ur_step_cell(0, pl, 1, &row, &col) && row == r && col == 3);
        CHECK(ur_step_cell(0, pl, 13, &row, &col) && row == r && col == 7);
    }
    /* shared squares: both players map step s (5..12) to the same cell */
    for (int s = 5; s <= 12; s++) {
        int r0, c0, r1, c1;
        ur_step_cell(0, 0, s, &r0, &c0);
        ur_step_cell(0, 1, s, &r1, &c1);
        CHECK(r0 == 1 && r1 == 1 && c0 == c1 && c0 == s - 5);
    }
}

/* ---- fuzz ---- */

typedef struct { long games, turns, moves, passes, captures, extras, bearoffs, maxturns; } FuzzStats;

static void check_invariants(const UrPos *p)
{
    CHECK(ur_pos_valid(p));
    for (int i = 0; i < 2; i++) {
        int n = 0;
        for (int s = 1; s <= UR_MAX_TRACK; s++) n += (p->mask[i] >> s) & 1;
        CHECK(n + p->pool[i] + p->done[i] == 7);
        CHECK((p->mask[i] >> (ur_off(p))) == 0 && !(p->mask[i] & 1u));   /* only bits 1..track */
    }
    /* literal expectations: shared steps are 5..12 (Ur) or 5..16 (Twenty) */
    uint32_t sh = (p->board == UR_BOARD_UR) ? 0x1FE0u : 0x1FFE0u;
    CHECK(((p->mask[0] & p->mask[1]) & sh) == 0);
}

static FuzzStats fuzz(int board, int games, uint64_t seed)
{
    FuzzStats st = {0};
    UrRng rng;
    ur_rng_seed(&rng, seed);
    for (int g = 0; g < games; g++) {
        UrPos p;
        ur_init(&p, (int)(ur_rng_u32(&rng) & 1), board);
        long turns = 0;
        while (ur_winner(&p) < 0) {
            int roll = ur_roll(&rng);
            UrMove mv[UR_MAX_MOVES];
            int n = ur_gen_moves(&p, roll, mv);
            CHECK(n >= 0 && n <= UR_MAX_MOVES);
            st.turns++;
            if (++turns > 5000) { CHECK(!"game too long"); break; }
            if (n == 0) {
                ur_pass(&p);
                st.passes++;
                check_invariants(&p);
                continue;
            }
            for (int i = 1; i < n; i++) CHECK(mv[i].from > mv[i - 1].from);
            UrMove m = mv[ur_rng_below(&rng, (uint32_t)n)];
            CHECK(m.to == m.from + roll);
            UrPos before = p;
            UrMoveResult r = ur_apply(&p, m, roll);
            check_invariants(&p);
            st.moves++;
            st.captures += r.captured; st.extras += r.extra_turn; st.bearoffs += r.borne_off;
            CHECK(r.extra_turn == (p.turn == before.turn && r.winner < 0));
            {
                int off = board == UR_BOARD_UR ? 15 : 17;
                bool ros = board == UR_BOARD_UR ? (m.to == 4 || m.to == 8 || m.to == 14)
                                                : (m.to == 4 || m.to == 8 || m.to == 12 || m.to == 16);
                int hi = board == UR_BOARD_UR ? 12 : 16;
                bool safe = board == UR_BOARD_UR ? (m.to == 8) : (m.to == 8 || m.to == 12 || m.to == 16);
                CHECK(m.to <= off && m.from + roll == m.to);
                CHECK(r.extra_turn == ros);
                CHECK(r.captured == (m.to >= 5 && m.to <= hi && !safe && (before.mask[before.turn ^ 1] & (1u << m.to))));
                CHECK(!safe || !(before.mask[before.turn ^ 1] & (1u << m.to)));   /* never moved onto an enemy on a safe square */
            }
            CHECK(p.pool[before.turn ^ 1] == before.pool[before.turn ^ 1] + r.captured);
        }
        int w = ur_winner(&p);
        CHECK(w >= 0 && p.done[w] == 7 && p.done[w ^ 1] < 7);
        st.games++;
        if (turns > st.maxturns) st.maxturns = turns;
    }
    return st;
}

/* ---- AI ---- */

static void test_ai_basic(void)
{
    UrRng rng;
    ur_rng_seed(&rng, 99);
    /* always returns a legal move, at every level, on fuzzed positions */
    for (int g = 0; g < 300; g++) {
        UrPos p;
        ur_init(&p, g & 1, g_board);
        while (ur_winner(&p) < 0) {
            int roll = ur_roll(&rng);
            UrMove mv[UR_MAX_MOVES];
            int n = ur_gen_moves(&p, roll, mv);
            if (!n) { ur_pass(&p); continue; }
            UrLevel lv = (UrLevel)(ur_rng_below(&rng, 3));
            UrAiStats st;
            UrMove m = ur_ai_choose(&p, roll, lv, &rng, &st);
            bool ok = false;
            for (int i = 0; i < n; i++) ok |= (mv[i].from == m.from && mv[i].to == m.to);
            CHECK(ok);
            if (n == 1) CHECK(st.nodes == 0);
            CHECK(lv != UR_EXPERT || st.nodes <= UR_AI_DEFAULT.expert_budget);
            ur_apply(&p, m, roll);
        }
    }

    /* expert takes an immediate win */
    int fin = UR_BOARDS[g_board].track - 1;
    int a[] = {fin}, b[] = {9, 10, 11};
    UrPos p = mk(0, a, 1, 6, b, 3, 0);
    UrMove m = ur_ai_choose(&p, 2, UR_EXPERT, &rng, NULL);
    CHECK(m.from == fin && m.to == fin + 2);

    /* expert captures an advanced piece rather than shuffling a back piece */
    int a3[] = {7, 1}, b3[] = {9};
    p = mk(0, a3, 2, 0, b3, 1, 0);
    m = ur_ai_choose(&p, 2, UR_EXPERT, &rng, NULL);
    CHECK(m.from == 7 && m.to == 9);
    m = ur_ai_choose(&p, 2, UR_MEDIUM, &rng, NULL);
    CHECK(m.from == 7 && m.to == 9);
}

/* Alpha-beta/star1 pruning must give exactly the same value as plain expectiminimax. */
static void test_pruning_equivalence(void)
{
    UrRng rng;
    ur_rng_seed(&rng, 4242);
    int tested = 0;
    double worst = 0;
    long full_nodes = 0, pruned_nodes = 0;
    for (int g = 0; g < 40; g++) {
        UrPos p;
        ur_init(&p, g & 1, g_board);
        int steps = (int)ur_rng_below(&rng, 60);
        while (ur_winner(&p) < 0 && steps-- > 0) {
            int roll = ur_roll(&rng);
            UrMove mv[UR_MAX_MOVES];
            int n = ur_gen_moves(&p, roll, mv);
            if (!n) ur_pass(&p);
            else ur_apply(&p, mv[ur_rng_below(&rng, (uint32_t)n)], roll);
        }
        if (ur_winner(&p) >= 0) continue;
        for (int d = 1; d <= 3; d++) {
            uint32_t n1, n2;
            float v1 = ur_search_value(&p, p.turn, d, false, &UR_AI_DEFAULT.w, &n1);
            float v2 = ur_search_value(&p, p.turn, d, true, &UR_AI_DEFAULT.w, &n2);
            double diff = fabs((double)v1 - v2);
            if (diff > worst) worst = diff;
            CHECK(diff < 0.05);
            if (d == 3) { full_nodes += n1; pruned_nodes += n2; }
            tested++;
        }
    }
    printf("  pruning equivalence: %d positions/depths, max |diff| = %.5f; depth-3 nodes full %ld vs pruned %ld\n",
           tested, worst, full_nodes, pruned_nodes);
}

/* ---- Twenty Squares ---- */

static void test_twenty_rules(void)
{
    g_board = UR_BOARD_TWENTY;
    UrPos p;
    ur_init(&p, 0, UR_BOARD_TWENTY);
    CHECK(p.board == UR_BOARD_TWENTY && ur_off(&p) == 17 && ur_pos_valid(&p));
    CHECK(strcmp(UR_BOARDS[1].name, "Twenty Squares") == 0 && UR_BOARDS[1].cols == 12);

    /* table queries, literal lists */
    for (int s = 0; s <= 17; s++) {
        bool ros = (s == 4 || s == 8 || s == 12 || s == 16);
        bool shr = (s >= 5 && s <= 16);
        CHECK(ur_is_rosette(1, s) == ros);
        CHECK(ur_is_shared(1, s) == shr);
        CHECK(ur_is_safe(1, s) == (s == 8 || s == 12 || s == 16));
        bool uros = (s == 4 || s == 8 || s == 14);
        CHECK(ur_is_rosette(0, s) == uros);
        CHECK(ur_is_shared(0, s) == (s >= 5 && s <= 12));
        CHECK(ur_is_safe(0, s) == (s == 8));
    }

    /* each rosette gives an extra roll; 4 is private, the rest shared */
    int rs[] = {4, 8, 12, 16};
    for (int i = 0; i < 4; i++) {
        int a[] = {rs[i] - 1};
        p = mk(0, a, 1, 0, NULL, 0, 0);
        UrMoveResult r = ur_apply(&p, (UrMove){(uint8_t)(rs[i] - 1), (uint8_t)rs[i]}, 1);
        CHECK(r.extra_turn && p.turn == 0 && !r.captured && (p.mask[0] & (1u << rs[i])));
    }
    /* non-rosettes switch turn: 13, 14, 15 are plain shared squares here */
    for (int s = 13; s <= 15; s++) {
        int a[] = {s - 1};
        p = mk(0, a, 1, 0, NULL, 0, 0);
        UrMoveResult r = ur_apply(&p, (UrMove){(uint8_t)(s - 1), (uint8_t)s}, 1);
        CHECK(!r.extra_turn && p.turn == 1);
    }

    /* safe rosettes 8, 12 and 16: an enemy there blocks the move, for both players */
    int safes[] = {8, 12, 16};
    for (int i = 0; i < 3; i++) {
        int s = safes[i];
        for (int pl = 0; pl < 2; pl++) {
            int mine[] = {s - 2}, theirs[] = {s};
            UrPos q = pl == 0 ? mk(0, mine, 1, 0, theirs, 1, 0) : mk(1, theirs, 1, 0, mine, 1, 0);
            CHECK(!has_move(&q, 2, s - 2, s));
            CHECK(has_move(&q, 1, s - 2, s - 1));
        }
        /* with the safe square empty the move is fine; with my own piece it is blocked */
        int mine[] = {s - 2};
        p = mk(0, mine, 1, 0, NULL, 0, 0);
        CHECK(has_move(&p, 2, s - 2, s));
        int own[] = {s - 2, s};
        p = mk(0, own, 2, 0, NULL, 0, 0);
        CHECK(!has_move(&p, 2, s - 2, s));
    }
    /* entering from the pool onto the private rosette 4 */
    p = mk(0, NULL, 0, 0, NULL, 0, 0);
    CHECK(has_move(&p, 4, 0, 4));
    {
        UrMoveResult r = ur_apply(&p, (UrMove){0, 4}, 4);
        CHECK(r.extra_turn && p.turn == 0);
    }
    /* step 4 is private: same step number held by both is fine */
    int a4[] = {3}, b4[] = {4};
    p = mk(0, a4, 1, 0, b4, 1, 0);
    CHECK(has_move(&p, 1, 3, 4));
    CHECK(!ur_apply(&p, (UrMove){3, 4}, 1).captured);

    /* captures on every non-safe shared square 5..16 */
    for (int s = 5; s <= 16; s++) {
        if (s == 8 || s == 12 || s == 16) continue;
        for (int pl = 0; pl < 2; pl++) {
            int mine[] = {s - 1}, theirs[] = {s};
            UrPos q = pl == 0 ? mk(0, mine, 1, 0, theirs, 1, 0) : mk(1, theirs, 1, 0, mine, 1, 0);
            CHECK(has_move(&q, 1, s - 1, s));
            UrMoveResult r = ur_apply(&q, (UrMove){(uint8_t)(s - 1), (uint8_t)s}, 1);
            CHECK(r.captured && !r.extra_turn && q.pool[pl ^ 1] == 7 && q.mask[pl ^ 1] == 0);
            CHECK(ur_pos_valid(&q));
        }
    }

    /* exact bear-off at 17 */
    int a[] = {16, 15, 14, 13};
    p = mk(0, a, 4, 0, NULL, 0, 0);
    CHECK(has_move(&p, 1, 16, 17));
    CHECK(!has_move(&p, 2, 16, 18));
    CHECK(has_move(&p, 2, 15, 17));
    CHECK(has_move(&p, 3, 14, 17));
    CHECK(has_move(&p, 4, 13, 17));
    CHECK(!has_move(&p, 4, 14, 18) && !has_move(&p, 3, 15, 18) && !has_move(&p, 2, 16, 18));
    CHECK(!has_move(&p, 1, 15, 16));   /* own piece */
    {
        UrMoveResult r = ur_apply(&p, (UrMove){16, 17}, 1);
        CHECK(r.borne_off && !r.extra_turn && p.turn == 1 && p.done[0] == 1 && !(p.mask[0] & (1u << 16)));
        CHECK(!(p.mask[0] & (1u << 17)));
    }
    /* last piece off wins */
    int w[] = {15};
    p = mk(1, NULL, 0, 0, w, 1, 6);
    CHECK(ur_apply(&p, (UrMove){15, 17}, 2).winner == 1 && ur_winner(&p) == 1);
    /* lone last piece on 16: only a 1 moves */
    int l[] = {16};
    p = mk(0, l, 1, 6, NULL, 0, 0);
    CHECK(count_moves(&p, 1) == 1 && count_moves(&p, 2) == 0 && count_moves(&p, 3) == 0 && count_moves(&p, 4) == 0);
    /* a pos with a bit above the track is invalid; so is a shared overlap on 16 */
    p = mk(0, NULL, 0, 0, NULL, 0, 0);
    p.mask[0] = 1u << 17; p.pool[0] = 6;
    CHECK(!ur_pos_valid(&p));
    int o1[] = {16}, o2[] = {16};
    p = mk(0, o1, 1, 0, o2, 1, 0);
    CHECK(!ur_pos_valid(&p));
    /* on the Ur board step 15/16 do not exist */
    p = mk(0, NULL, 0, 0, NULL, 0, 0);
    p.board = UR_BOARD_UR; p.mask[0] = 1u << 15; p.pool[0] = 6;
    CHECK(!ur_pos_valid(&p));
    g_board = UR_BOARD_UR;
}

static void test_cells_all(void)
{
    /* the 3x12 cells of Twenty Squares: rows 0, 2 cols 0-3, row 1 cols 0-11 */
    for (int r = -1; r <= 3; r++)
        for (int c = -1; c <= 12; c++) {
            bool want = r >= 0 && r <= 2 && c >= 0 && c <= 11 && (r == 1 || c <= 3);
            CHECK(ur_cell_exists(1, r, c) == want);
            bool uwant = r >= 0 && r <= 2 && c >= 0 && c <= 7 && (r == 1 || c <= 3 || c >= 6);
            CHECK(ur_cell_exists(0, r, c) == uwant);
        }
    int n = 0;
    for (int r = 0; r < 3; r++) for (int c = 0; c < 12; c++) n += ur_cell_exists(1, r, c);
    CHECK(n == 20);
    n = 0;
    for (int r = 0; r < 3; r++) for (int c = 0; c < 8; c++) n += ur_cell_exists(0, r, c);
    CHECK(n == 20);

    for (int pl = 0; pl < 2; pl++) {
        int r0 = pl == 0 ? 2 : 0, row, col;
        int seen[3][12];
        memset(seen, 0, sizeof seen);
        CHECK(!ur_step_cell(1, pl, 0, &row, &col) && !ur_step_cell(1, pl, 17, &row, &col));
        for (int s = 1; s <= 16; s++) {
            CHECK(ur_step_cell(1, pl, s, &row, &col));
            CHECK(ur_cell_exists(1, row, col));
            CHECK(ur_cell_step(1, pl, row, col) == s);
            seen[row][col]++;
            /* literal cell list */
            if (s <= 4) CHECK(row == r0 && col == 4 - s);
            else        CHECK(row == 1 && col == s - 5);
        }
        for (int r = 0; r < 3; r++) for (int c = 0; c < 12; c++) CHECK(seen[r][c] <= 1);
        int other = pl == 0 ? 0 : 2;
        for (int c = -1; c < 13; c++) CHECK(ur_cell_step(1, pl, other, c) == -1);
        CHECK(ur_cell_step(1, pl, 1, 12) == -1 && ur_cell_step(1, pl, 3, 0) == -1);
        /* the outer row cols 4..11 of the player's own row do not exist */
        for (int c = 4; c < 12; c++) CHECK(ur_cell_step(1, pl, r0, c) == -1);
        /* rosette cells: (r,0) (1,3) (1,7) (1,11) */
        for (int s = 1; s <= 16; s++) {
            ur_step_cell(1, pl, s, &row, &col);
            bool cell_ros = (row == r0 && col == 0) || (row == 1 && (col == 3 || col == 7 || col == 11));
            CHECK(cell_ros == ur_is_rosette(1, s));
        }
    }
    for (int s = 5; s <= 16; s++) {
        int a, b, c, d;
        ur_step_cell(1, 0, s, &a, &b);
        ur_step_cell(1, 1, s, &c, &d);
        CHECK(a == 1 && c == 1 && b == d && b == s - 5);
    }
    /* Ur rosette cells for both players */
    for (int pl = 0; pl < 2; pl++) {
        int r0 = pl == 0 ? 2 : 0, row, col;
        int cnt = 0;
        for (int s = 1; s <= 14; s++) {
            ur_step_cell(0, pl, s, &row, &col);
            bool cell_ros = (row == r0 && (col == 0 || col == 6)) || (row == 1 && col == 3);
            CHECK(cell_ros == ur_is_rosette(0, s));
            cnt += cell_ros;
        }
        CHECK(cnt == 3);
    }
}

int main(int argc, char **argv)
{
    int fuzz_games = 20000;
    if (argc > 1) fuzz_games = atoi(argv[1]);

    test_init_and_dice();
    test_rosettes();
    test_central_rosette();
    test_capture();
    test_bearoff_and_blocking();
    test_pass_and_extra_roll();
    test_extra_roll_dead();
    test_win();
    test_cells();
    test_twenty_rules();
    test_cells_all();
    int unit_checks = g_checks, unit_fail = g_fail;
    printf("unit tests: %d checks, %d failed\n", unit_checks, unit_fail);

    for (int b = 0; b < UR_BOARD_COUNT; b++) {
        int c0 = g_checks, f0 = g_fail;
        FuzzStats st = fuzz(b, fuzz_games, 20261026 + b);
        printf("fuzz %s: %ld games, %ld rolls, %ld moves, %ld passes, %ld captures, %ld extra rolls, %ld bear-offs, longest game %ld rolls\n",
               UR_BOARDS[b].name, st.games, st.turns, st.moves, st.passes, st.captures, st.extras, st.bearoffs, st.maxturns);
        printf("fuzz %s checks: %d checks, %d failed\n", UR_BOARDS[b].name, g_checks - c0, g_fail - f0);
    }

    for (int b = 0; b < UR_BOARD_COUNT; b++) {
        g_board = b;
        int before = g_checks, bf = g_fail;
        test_ai_basic();
        test_pruning_equivalence();
        printf("ai tests (%s): %d checks, %d failed\n", UR_BOARDS[b].name, g_checks - before, g_fail - bf);
    }
    g_board = UR_BOARD_UR;

    printf("TOTAL: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
