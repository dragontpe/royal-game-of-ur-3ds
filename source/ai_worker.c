#include <3ds.h>
#include <string.h>

#include "ai_worker.h"

/* The worker runs at a lower priority than the main thread on the same
 * core, so it only gets the CPU while the main thread waits for vblank. */
static Thread thread;
static LightEvent wake;
static volatile bool quit;
static volatile int state; /* 0 idle, 1 job queued, 2 result ready */
static UrPos job_pos;
static int job_roll;
static UrLevel job_level;
static UrRng job_rng;
static UrMove result;
static UrAiStats result_stats;
static float result_ms;

static void worker(void *arg)
{
	(void)arg;
	while (!quit) {
		LightEvent_Wait(&wake);
		if (quit)
			break;
		if (state != 1)
			continue;
		u64 t0 = svcGetSystemTick();
		UrAiStats st;
		memset(&st, 0, sizeof(st));
		UrMove m = ur_ai_choose(&job_pos, job_roll, job_level, &job_rng, &st);
		result = m;
		result_stats = st;
		result_ms = (svcGetSystemTick() - t0) / (SYSCLOCK_ARM11 / 1000.0f);
		__atomic_store_n(&state, 2, __ATOMIC_RELEASE);
	}
}

bool ai_worker_init(void)
{
	s32 prio = 0x30;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	LightEvent_Init(&wake, RESET_ONESHOT);
	quit = false;
	state = 0;
	thread = threadCreate(worker, NULL, 64 * 1024, prio + 1, -2, false);
	return thread != NULL;
}

void ai_worker_exit(void)
{
	if (!thread)
		return;
	quit = true;
	LightEvent_Signal(&wake);
	threadJoin(thread, U64_MAX);
	threadFree(thread);
	thread = NULL;
}

void ai_worker_start(const UrPos *p, int roll, UrLevel level, u64 seed)
{
	job_pos = *p;
	job_roll = roll;
	job_level = level;
	ur_rng_seed(&job_rng, seed);
	__atomic_store_n(&state, 1, __ATOMIC_RELEASE);
	LightEvent_Signal(&wake);
}

bool ai_worker_poll(UrMove *move, UrAiStats *stats, float *ms)
{
	if (__atomic_load_n(&state, __ATOMIC_ACQUIRE) != 2)
		return false;
	*move = result;
	if (stats)
		*stats = result_stats;
	if (ms)
		*ms = result_ms;
	state = 0;
	return true;
}
