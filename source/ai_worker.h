/* Runs the AI's move search on a worker thread so the screens keep
 * animating while the Expert thinks. */
#ifndef UR_AI_WORKER_H
#define UR_AI_WORKER_H

#include <stdbool.h>
#include "ur_ai.h"

bool ai_worker_init(void);
void ai_worker_exit(void);
void ai_worker_start(const UrPos *p, int roll, UrLevel level, u64 seed);
/* true once the move is ready; fills *move and the search stats. */
bool ai_worker_poll(UrMove *move, UrAiStats *stats, float *ms);

#endif
