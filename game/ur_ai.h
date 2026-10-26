/* Royal Game of Ur AI: expectiminimax over the 5 dice outcomes. Portable C99, no heap. */
#ifndef UR_AI_H
#define UR_AI_H

#include "ur_rules.h"

typedef enum { UR_BEGINNER = 0, UR_MEDIUM = 1, UR_EXPERT = 2 } UrLevel;

typedef struct {
    uint32_t nodes;     /* chance nodes visited (incl. leaf evals), 0 if the move was forced */
    int      depth;     /* deepest fully searched depth in moves, 0 for static choices */
} UrAiStats;

/* Static evaluation weights (all in "points"; a step of progress is `step`). */
typedef struct {
    float step;         /* per step of progress */
    float done;         /* extra for each borne-off piece */
    float centre;       /* holding the central rosette */
    float danger;       /* scale for expected loss from capture threats */
    float loss;         /* extra cost of losing a piece, on top of its progress */
    float defer;        /* danger multiplier for the side to move (it can run away) */
    float pool;         /* penalty per piece still in the start pool */
} UrEvalW;

typedef struct {
    UrEvalW  w;
    uint32_t expert_budget;     /* node budget per move */
    int      expert_max_depth;
    int      medium_depth;
    float    medium_noise;      /* +-noise added to each root value */
    float    beginner_noise;    /* +-noise added to each 1-ply score */
    int      beginner_random;   /* percent of moves picked at random */
} UrAiConfig;

extern const UrAiConfig UR_AI_DEFAULT;

/* Static evaluation from `me`'s point of view (positive = good for me). */
float ur_eval(const UrPos *p, int me, const UrEvalW *w);

/* Pick a move for p->turn. Call only when there is at least one legal move. */
UrMove ur_ai_choose(const UrPos *p, int roll, UrLevel level, UrRng *rng, UrAiStats *stats_or_null);

/* Same with explicit tuning. */
UrMove ur_ai_choose_cfg(const UrPos *p, int roll, UrLevel level, UrRng *rng,
                        const UrAiConfig *cfg, UrAiStats *stats_or_null);

/* Expected value of a position before the roll (depth in moves), from `me`'s view.
   prune=false gives the plain full-width search (used by tests to check pruning). */
float ur_search_value(const UrPos *p, int me, int depth, bool prune, const UrEvalW *w, uint32_t *nodes);

#endif
