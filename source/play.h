/* One game of Ur: turn flow, input, the in-game bottom screen. */
#ifndef UR_PLAY_H
#define UR_PLAY_H

#include <3ds.h>
#include <stdbool.h>
#include "ur_ai.h"

typedef enum { MODE_AI, MODE_2P } PlayMode;

typedef struct {
	u32 down, held;
	bool touch_down;     /* stylus went down this frame */
	int tx, ty;
} Input;

typedef struct {
	bool sound, music, auto_move;
	u8 last_level;
	u16 wins[3], losses[3];   /* vs Beginner / Medium / Expert */
	u16 games_2p;
} Settings;

extern Settings settings;

enum { PLAY_RUNNING, PLAY_QUIT, PLAY_RULES };

void play_init(void);
void play_new(PlayMode mode, UrLevel level);
bool play_has_save(void);
bool play_resume(void);
int  play_update(float dt, const Input *in);     /* PLAY_* */
void play_draw_bottom(void);
void play_draw_top(void);                         /* 2D overlay over the 3D, per eye */
bool play_room_view(void);                        /* camera should show the room (game over) */
void play_settings_save(void);

/* Test hooks (robot builds) */
enum {
	PH_INTRO, PH_TURN, PH_WAIT_ROLL, PH_AI_PREROLL, PH_ROLLING, PH_PICK, PH_AUTO, PH_AI_THINK, PH_AI_SHOW,
	PH_MOVING, PH_AFTER, PH_PASS, PH_OVER
};
typedef struct {
	int phase, nmoves, turn, roll, winner, mode, level;
	bool paused;
	UrMove moves[UR_MAX_MOVES];
	const char *msg, *sub;
	UrPos pos;
	float ai_ms;
	u32 ai_nodes;
} PlayDebug;
void play_debug(PlayDebug *d);
bool play_over(void);
int  play_winner(void);

#endif
