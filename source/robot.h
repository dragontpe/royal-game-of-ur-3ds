/* Robot player for UR_ROBOT builds (make ROBOT=1). Included by main.c.
 *
 * Drives the real UI with synthetic presses and taps: tours every menu
 * page, then plays games against each AI level and one two-player game,
 * mixing stylus taps and D-pad play. Checks the rules state every frame,
 * watches for stalls, saves screenshots to sdmc:/3ds/ur-3ds/shots/ and a
 * log to sdmc:/3ds/ur-3ds/robot.txt. Saves go to the fuzz folder. */
#include <stdarg.h>

#define ROBOT_SPEED 3.0f
#define ROBOT_AI_GAMES 6

static FILE *rlog;
static float r_wait = 3.5f, r_still;
static int r_step, r_games, r_errors, r_shot_delay = -1;
static char r_shot_name[40];
static u32 r_seed = 20261026;
static bool r_done, r_paused_once, r_seen[8];
static int r_last_phase = -1, r_turns, r_level;
static char r_last_msg[64];
static float r_ai_ms_sum[3], r_ai_ms_max[3];
static int r_ai_n[3];

typedef struct {
	u32 keys;
	int tx, ty;
	bool touch;
} RAction;
static RAction r_queue[16];
static int r_qn, r_qi;

static void rlogf(const char *fmt, ...)
{
	if (!rlog) {
		mkdir("sdmc:/3ds", 0777);
		mkdir("sdmc:/3ds/ur-3ds", 0777);
		rlog = fopen("sdmc:/3ds/ur-3ds/robot.txt", "w");
	}
	if (!rlog)
		return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(rlog, fmt, ap);
	va_end(ap);
	fputc('\n', rlog);
	fflush(rlog);
}

static u32 rrand(u32 n)
{
	r_seed = r_seed * 1103515245u + 12345u;
	return (r_seed >> 16) % n;
}

static void rshot(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(r_shot_name, sizeof(r_shot_name), fmt, ap);
	va_end(ap);
	r_shot_delay = 4;
}

static void q_key(u32 k)
{
	if (r_qn < 16)
		r_queue[r_qn++] = (RAction){ k, 0, 0, false };
}

static void q_tap(int x, int y)
{
	if (r_qn < 16)
		r_queue[r_qn++] = (RAction){ 0, x, y, true };
}

static void step_xy(int player, int step, int *x, int *y)
{
	int row_y = player == 0 ? 148 : 6, row, col;
	if (step == 0) {
		*x = 26;
		*y = row_y + 13;
	} else if (step == UR_OFF) {
		*x = 284;
		*y = row_y + 13;
	} else {
		ur_step_cell(player, step, &row, &col);
		*x = 24 + col * 34 + 17;
		*y = 42 + row * 34 + 17;
	}
}

static void robot_menu(int scr)
{
	/* the tour, then games; each step queues actions and a wait */
	switch (r_step) {
	case 0: rshot("01_title"); r_wait = 1.0f; break;
	case 1: q_tap(80, 190); r_wait = 1.2f; break;                        /* The story */
	case 2: case 3: case 4: case 5: case 6: case 7:
		rshot("02_story_%d", r_step - 1);
		if (r_step < 7)
			q_key(KEY_R);
		else
			q_key(KEY_B);
		r_wait = 0.8f;
		break;
	case 8: q_tap(230, 130); r_wait = 1.2f; break;                       /* How to play */
	case 9: case 10: case 11: case 12: case 13: case 14:
		rshot("03_rules_%d", r_step - 8);
		if (r_step < 14)
			q_tap(276, 214);
		else
			q_tap(56, 214);
		r_wait = 0.8f;
		break;
	case 15: q_tap(230, 190); r_wait = 1.0f; break;                      /* Settings */
	case 16: rshot("04_settings"); q_key(KEY_DDOWN); q_key(KEY_DDOWN); q_key(KEY_A); r_wait = 0.8f; break;
	case 17: rshot("04_settings_toggled"); q_key(KEY_A); q_key(KEY_B); r_wait = 0.8f; break;
	default:
		if (scr != SCR_TITLE)
			break;
		if (r_games < ROBOT_AI_GAMES) {
			r_level = r_games % 3;
			q_tap(230, 70);                                                  /* Play */
			r_step = 100;
			r_wait = 1.0f;
		} else if (r_games == ROBOT_AI_GAMES) {
			q_tap(80, 130);                                                  /* Two players */
			r_wait = 1.0f;
		} else if (!r_done) {
			r_done = true;
			rshot("99_end_title");
			rlogf("ROBOT DONE: %d games, %d errors", r_games, r_errors);
			for (int i = 0; i < 3; i++)
				if (r_ai_n[i])
					rlogf("AI level %d: %d moves, avg %.1f ms, max %.1f ms", i, r_ai_n[i],
					      r_ai_ms_sum[i] / r_ai_n[i], r_ai_ms_max[i]);
		}
		return;
	}
	r_step++;
}

static void robot_level(void)
{
	if (r_step == 100 && r_games == 0)
		rshot("05_level");
	q_tap(160, 44 + r_level * 52 + 22);
	r_wait = 1.0f;
	r_step = 101;
}

static void robot_play(void)
{
	PlayDebug d;
	play_debug(&d);

	if (!ur_pos_valid(&d.pos)) {
		r_errors++;
		rlogf("ERROR invalid position, game %d", r_games);
	}
	if (d.phase != r_last_phase || strcmp(d.msg, r_last_msg) != 0) {
		r_last_phase = d.phase;
		snprintf(r_last_msg, sizeof(r_last_msg), "%s", d.msg);
		r_still = 0;
		if (d.phase == PH_TURN)
			r_turns++;
		if (d.phase == PH_AI_SHOW && d.mode == MODE_AI) {
			int l = d.level;
			r_ai_ms_sum[l] += d.ai_ms;
			if (d.ai_ms > r_ai_ms_max[l])
				r_ai_ms_max[l] = d.ai_ms;
			r_ai_n[l]++;
		}
		if (!strncmp(d.msg, "Rosette", 7) && !r_seen[0]) {
			r_seen[0] = true;
			rshot("10_rosette");
		}
		if (!strcmp(d.msg, "Captured!") && !r_seen[1]) {
			r_seen[1] = true;
			rshot("11_capture");
		}
		if (d.phase == PH_AI_THINK && !r_seen[2]) {
			r_seen[2] = true;
			rshot("12_ai_thinking");
		}
		if (d.phase == PH_WAIT_ROLL && !r_seen[3] && r_games == 0) {
			r_seen[3] = true;
			rshot("06_your_turn");
		}
		if (d.phase == PH_PASS && !r_seen[4]) {
			r_seen[4] = true;
			rshot("13_no_move");
		}
		if (d.mode == MODE_2P && d.phase == PH_WAIT_ROLL && !r_seen[5]) {
			r_seen[5] = true;
			rshot("20_two_players");
		}
	}
	if (d.phase == PH_OVER) {
		if (r_wait > 0)
			return;
		if (r_still < 1.5f)
			return;
		rlogf("game %d (%s level %d): winner %d, home %d-%d, %d turns", r_games,
		      d.mode == MODE_AI ? "AI" : "2P", d.level, d.winner, d.pos.done[0], d.pos.done[1], r_turns);
		rshot("30_over_%d", r_games);
		r_games++;
		r_turns = 0;
		q_key(KEY_B);
		r_wait = 1.0f;
		r_step = 18;
		return;
	}
	if (r_still > 25.0f) {
		r_errors++;
		rlogf("ERROR stalled in phase %d msg '%s' game %d", d.phase, d.msg, r_games);
		rshot("90_stall_%d", r_errors);
		r_still = 0;
	}
	if (r_wait > 0 || d.paused)
		return;

	if (d.phase == PH_WAIT_ROLL) {
		if (!r_paused_once && r_games == 0 && r_turns > 6) {
			r_paused_once = true;
			q_key(KEY_START);
			r_wait = 0.6f;
			return;
		}
		if (rrand(2))
			q_key(KEY_A);
		else
			q_tap(252, 209);
		r_wait = 0.3f + rrand(30) / 100.0f;
	} else if (d.phase == PH_PICK && d.nmoves > 0) {
		if (!r_seen[6] && d.nmoves > 1) {
			r_seen[6] = true;
			rshot("07_pick");
			r_wait = 0.5f;
			return;
		}
		int i = rrand(d.nmoves), how = rrand(10), x, y;
		if (how < 4) {
			step_xy(d.turn, d.moves[i].from, &x, &y);
			q_tap(x, y);
		} else if (how < 7) {
			step_xy(d.turn, d.moves[i].to, &x, &y);
			/* a target that is also another move's piece picks that move: fine either way */
			q_tap(x, y);
		} else {
			for (int k = 0; k <= i; k++)
				q_key(KEY_DRIGHT);
			if (!r_seen[7]) {
				r_seen[7] = true;
				rshot("08_dpad_select");
			}
			q_key(KEY_A);
		}
		r_wait = 0.4f + rrand(30) / 100.0f;
	}
}

static void robot_input(Input *in, int scr, float dt)
{
	if (r_shot_delay > 0)
		return;
	r_wait -= dt * ROBOT_SPEED;
	r_still += dt * ROBOT_SPEED;
	if (r_qi < r_qn) {
		static float gap;
		gap -= dt;
		if (gap > 0)
			return;
		gap = 0.12f;
		RAction a = r_queue[r_qi++];
		in->down |= a.keys;
		if (a.touch) {
			in->touch_down = true;
			in->tx = a.tx;
			in->ty = a.ty;
		}
		if (r_qi == r_qn)
			r_qi = r_qn = 0;
		return;
	}
	if (scr == SCR_PLAY) {
		PlayDebug d;
		play_debug(&d);
		if (d.paused) {
			if (r_wait <= 0) {
				rshot("09_pause");
				q_key(KEY_START);
				r_wait = 0.6f;
			}
			return;
		}
		robot_play();
		return;
	}
	if (scr == SCR_SPLASH) {
		static bool shot;
		if (r_wait > 0)
			return;
		if (!shot) {
			shot = true;
			rshot("00_splash");
			r_wait = 1.0f;
		} else {
			q_key(KEY_A);
			r_wait = 2.0f;
		}
		return;
	}
	if (r_wait > 0 || r_done)
		return;
	if (scr == SCR_LEVEL)
		robot_level();
	else
		robot_menu(scr);
}

static void robot_after_frame(int scr)
{
	(void)scr;
	if (r_shot_delay > 0 && --r_shot_delay == 0) {
		shot_save(r_shot_name);
		r_shot_delay = -1;
	}
}
