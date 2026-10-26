/* Emits a transcript of random games for the Python cross-check (check.py).
   usage: xcheck_gen <games> <seed>
   Per game:  G <index> <first player> <policy>
   Per roll:  M <player> <roll> <n> <from:to ...> <chosen from:to | pass>
              S <turn> <mask0> <mask1> <pool0> <pool1> <done0> <done1> <winner>   (state after) */
#include <stdio.h>
#include <stdlib.h>
#include "../../game/ur_rules.h"
#include "../../game/ur_ai.h"

static void print_state(const UrPos *p)
{
    printf("S %d %u %u %d %d %d %d %d\n", p->turn, (unsigned)p->mask[0], (unsigned)p->mask[1],
           p->pool[0], p->pool[1], p->done[0], p->done[1], ur_winner(p));
}

int main(int argc, char **argv)
{
    int games = argc > 1 ? atoi(argv[1]) : 1000;
    uint64_t seed = argc > 2 ? strtoull(argv[2], 0, 10) : 1;
    UrRng rng;
    UrAiConfig fast = UR_AI_DEFAULT;
    fast.expert_budget = 2000;
    ur_rng_seed(&rng, seed);

    for (int g = 0; g < games; g++) {
        int policy = g % 8;    /* 0-3,5 random, 4 beginner, 6 medium, 7 expert */
        int first = 0;   /* the library always lets LIGHT (player 0) start */
        UrPos p;
        ur_init(&p, first);
        printf("G %d %d %d\n", g, first, policy);
        print_state(&p);
        while (ur_winner(&p) < 0) {
            int roll = ur_roll(&rng);
            UrMove mv[UR_MAX_MOVES];
            int n = ur_gen_moves(&p, roll, mv);
            printf("M %d %d %d", p.turn, roll, n);
            for (int i = 0; i < n; i++)
                printf(" %d:%d", mv[i].from, mv[i].to);
            if (n == 0) {
                printf(" pass\n");
                ur_pass(&p);
            } else {
                UrMove m;
                if (policy == 4)      m = ur_ai_choose_cfg(&p, roll, UR_BEGINNER, &rng, &fast, NULL);
                else if (policy == 6) m = ur_ai_choose_cfg(&p, roll, UR_MEDIUM, &rng, &fast, NULL);
                else if (policy == 7) m = ur_ai_choose_cfg(&p, roll, UR_EXPERT, &rng, &fast, NULL);
                else                  m = mv[ur_rng_below(&rng, (uint32_t)n)];
                printf(" %d:%d\n", m.from, m.to);
                ur_apply(&p, m, roll);
            }
            print_state(&p);
        }
    }
    return 0;
}
