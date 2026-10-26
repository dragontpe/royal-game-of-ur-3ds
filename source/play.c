/* One game of Ur. See play.h.
 *
 * Flow per turn: TURN (save) -> roll (button or AI) -> ROLLING (dice
 * tumble) -> PICK / AI_THINK / PASS -> MOVING (hops) -> TURN again, or OVER.
 * The rules live in game/ur_rules; this file only sequences them. */
#include "play.h"

#include <stdio.h>
#include <string.h>

#include "ai_worker.h"
#include "draw.h"
#include "save.h"
#include "sfx.h"
#include "world.h"

Settings settings = { true, true, true, UR_MEDIUM, { 0 }, { 0 }, 0 };

typedef struct {
	u8 mode, level, pad[2];
	UrPos pos;
	UrRng rng;
} GameSave;


/* Mini board on the bottom screen (matches tools/art/shapes.py). */
#define MAP_X 24
#define MAP_Y 42
#define CELL 34
#define ROW_OPP 6
#define ROW_YOU 148
#define HOME_X 264
#define HOME_W 40

static struct {
	PlayMode mode;
	UrLevel level;
	UrPos pos;
	UrRng rng;
	int phase;
	float timer, clock;
	int roll;
	bool marked[4];
	UrMove moves[UR_MAX_MOVES];
	int nmoves, sel;
	bool sel_by_pad;
	UrMoveResult last;
	char msg[64], sub[96];
	bool sub_quote;
	int winner;
	bool paused;
	int pause_sel, over_sel;
	float over_t;
	int fortune;
	float ai_ms;
	u32 ai_nodes;
} G;

static const char *const level_name[3] = { "Beginner", "Medium", "Expert" };

/* From the Babylonian rules tablet BM 33333B (Finkel's translation). */
static const char *const fortunes[] = {
	"\"You will find a friend.\"",
	"\"You will stand in exalted places.\"",
	"\"You will be powerful like a lion.\"",
	"\"You will draw fine beer.\"",
	"\"You will go up the path.\"",
	"\"You will cut meat.\"",
	"\"You will cross the ditch.\"",
	"\"There will be an abundance of fine beer.\"",
	"\"The pack will eat its fill of meat.\"",
	"\"A woman will love those who linger in a tavern.\"",
};
#define NFORTUNES (int)(sizeof(fortunes) / sizeof(fortunes[0]))

static bool is_human(int player)
{
	return G.mode == MODE_2P || player == 0;
}

static const char *name_of(int player)
{
	if (G.mode == MODE_2P)
		return player == 0 ? "Player 1" : "Player 2";
	return player == 0 ? "You" : level_name[G.level];
}

static void say(const char *msg, const char *sub, bool quote)
{
	snprintf(G.msg, sizeof(G.msg), "%s", msg);
	snprintf(G.sub, sizeof(G.sub), "%s", sub ? sub : "");
	G.sub_quote = quote;
}

static void game_save(void)
{
	GameSave s;
	memset(&s, 0, sizeof(s));
	s.mode = G.mode;
	s.level = G.level;
	s.pos = G.pos;
	s.rng = G.rng;
	save_write("game", &s, sizeof(s));
}

void play_settings_save(void)
{
	save_write("settings", &settings, sizeof(settings));
}

void play_init(void)
{
	Settings s;
	if (save_read("settings", &s, sizeof(s)) == 0)
		settings = s;
	if (settings.last_level > UR_EXPERT)
		settings.last_level = UR_MEDIUM;
}

bool play_has_save(void)
{
	GameSave s;
	return save_read("game", &s, sizeof(s)) == 0 && ur_pos_valid(&s.pos) && ur_winner(&s.pos) < 0;
}

static void start_common(void)
{
	G.phase = PH_INTRO;
	G.timer = 1.1f;
	G.paused = false;
	G.winner = -1;
	G.nmoves = 0;
	G.roll = -1;
	G.over_t = 0;
	world_reset(&G.pos);
	world_clear_dice();
	u8 none[16] = { 0 };
	world_highlight(0, none);
}

void play_new(PlayMode mode, UrLevel level)
{
	memset(&G, 0, sizeof(G));
	G.mode = mode;
	G.level = level;
	ur_rng_seed(&G.rng, svcGetSystemTick() ^ 0x5eedull);
	G.fortune = ur_rng_below(&G.rng, NFORTUNES);
	ur_init(&G.pos, ur_rng_below(&G.rng, 2));
	start_common();
	char m[48];
	snprintf(m, sizeof(m), G.pos.turn == 0 && mode == MODE_AI ? "You go first" : "%s goes first",
	         name_of(G.pos.turn));
	say(m, mode == MODE_AI ? "Race all seven pieces home" : "Pass the 3DS between turns", false);
	if (mode == MODE_AI) {
		settings.last_level = level;
		play_settings_save();
	}
}

bool play_resume(void)
{
	GameSave s;
	if (save_read("game", &s, sizeof(s)) != 0 || !ur_pos_valid(&s.pos))
		return false;
	memset(&G, 0, sizeof(G));
	G.mode = s.mode;
	G.level = s.level;
	G.pos = s.pos;
	G.rng = s.rng;
	G.fortune = ur_rng_below(&G.rng, NFORTUNES);
	start_common();
	G.timer = 0.9f;
	say("Welcome back", "The game is where you left it", false);
	return true;
}

/* ---- turn flow ---- */

static void do_roll(void)
{
	int player = G.pos.turn;
	G.roll = 0;
	for (int i = 0; i < 4; i++) {
		G.marked[i] = ur_rng_below(&G.rng, 2) != 0;
		G.roll += G.marked[i];
	}
	world_roll(player, G.marked);
	G.nmoves = 0;
	G.phase = PH_ROLLING;
	G.timer = 0.15f;
	char m[48];
	snprintf(m, sizeof(m), is_human(player) && G.mode == MODE_AI ? "Rolling..." : "%s rolls", name_of(player));
	say(m, "", false);
}

static void highlight(void)
{
	u8 f[16] = { 0 };
	if (G.phase == PH_PICK || G.phase == PH_AUTO || G.phase == PH_AI_SHOW) {
		for (int i = 0; i < G.nmoves; i++) {
			f[G.moves[i].from] |= HL_MOVABLE;
			f[G.moves[i].to] |= HL_TARGET;
		}
		if (G.sel >= 0 && G.sel < G.nmoves && (G.sel_by_pad || G.phase != PH_PICK)) {
			f[G.moves[G.sel].from] |= HL_SELECTED;
			f[G.moves[G.sel].to] |= HL_SELECTED;
		}
		if (G.phase == PH_AI_SHOW) {
			/* the AI only shows the move it picked */
			memset(f, 0, sizeof(f));
			f[G.moves[G.sel].from] = HL_MOVABLE | HL_SELECTED;
			f[G.moves[G.sel].to] = HL_TARGET | HL_SELECTED;
		}
	}
	world_highlight(G.pos.turn, f);
}

static void execute(int i)
{
	G.last = ur_apply(&G.pos, G.moves[i], G.roll);
	world_move(&G.last);
	G.nmoves = 0;
	G.phase = PH_MOVING;
	highlight();
}

static void after_move(void)
{
	const UrMoveResult *r = &G.last;
	int p = r->player;
	char m[64];
	if (r->winner >= 0) {
		G.winner = r->winner;
		G.phase = PH_OVER;
		G.over_t = 0;
		G.over_sel = 0;
		save_delete("game");
		bool human_won = G.mode == MODE_2P || G.winner == 0;
		sfx_play(human_won ? SFX_WIN : SFX_LOSE, 0, 1);
		if (G.mode == MODE_AI) {
			if (G.winner == 0)
				settings.wins[G.level]++;
			else
				settings.losses[G.level]++;
		} else {
			settings.games_2p++;
		}
		play_settings_save();
		return;
	}
	if (r->extra_turn) {
		sfx_play(SFX_ROSETTE, 0, 0.9f);
		if (is_human(p) && G.mode == MODE_AI) {
			G.fortune = (G.fortune + 1 + ur_rng_below(&G.rng, NFORTUNES - 1)) % NFORTUNES;
			say("Rosette! Roll again", fortunes[G.fortune], true);
		} else {
			snprintf(m, sizeof(m), "%s rolls again", name_of(p));
			say("Rosette!", m, false);
		}
	} else if (r->captured) {
		if (G.mode == MODE_AI)
			say("Captured!", p == 0 ? "Their piece goes back to the start" : "Your piece goes back to the start",
			    false);
		else {
			snprintf(m, sizeof(m), "%s captured a piece", name_of(p));
			say("Captured!", m, false);
		}
	} else if (r->borne_off) {
		snprintf(m, sizeof(m), "%d of 7 home", G.pos.done[p]);
		say(p == 0 && G.mode == MODE_AI ? "Piece home" : "Home", m, false);
	}
	G.phase = PH_AFTER;
	G.timer = r->extra_turn || r->captured ? 0.7f : 0.25f;
}

static void begin_turn(void)
{
	int p = G.pos.turn;
	game_save();
	G.roll = -1;
	if (is_human(p)) {
		G.phase = PH_WAIT_ROLL;
		if (!G.msg[0] || G.last.player != p || !G.last.extra_turn) {
			char m[48];
			snprintf(m, sizeof(m), G.mode == MODE_AI ? "Your turn" : "%s's turn", name_of(p));
			say(m, "Tap Roll or press A", false);
		}
	} else {
		G.phase = PH_AI_PREROLL;
		G.timer = 0.55f;
		char m[48];
		snprintf(m, sizeof(m), "%s's turn", name_of(p));
		if (!(G.last.player == p && G.last.extra_turn))
			say(m, "", false);
	}
}

static void after_roll(void)
{
	int p = G.pos.turn;
	char m[48];
	if (is_human(p) && G.mode == MODE_AI)
		snprintf(m, sizeof(m), "Rolled %d", G.roll);
	else
		snprintf(m, sizeof(m), "%s rolled %d", name_of(p), G.roll);
	G.nmoves = ur_gen_moves(&G.pos, G.roll, G.moves);
	if (G.nmoves == 0) {
		say(m, G.roll == 0 ? "No move this turn" : "No legal move", false);
		G.phase = PH_PASS;
		G.timer = 1.2f;
		return;
	}
	G.sel = 0;
	G.sel_by_pad = false;
	if (!is_human(p)) {
		ai_worker_start(&G.pos, G.roll, G.level, ur_rng_u32(&G.rng) | ((u64)ur_rng_u32(&G.rng) << 32));
		G.phase = PH_AI_THINK;
		G.timer = 0.5f;
		say(m, "Thinking...", false);
	} else if (G.nmoves == 1 && settings.auto_move) {
		G.phase = PH_AUTO;
		G.timer = 0.6f;
		say(m, "Only one move", false);
	} else {
		G.phase = PH_PICK;
		say(m, "Tap a piece to move it", false);
	}
	highlight();
}

/* ---- input ---- */

static int cell_center_x(int col) { return MAP_X + col * CELL + CELL / 2; }
static int cell_center_y(int row) { return MAP_Y + row * CELL + CELL / 2; }

/* Which move does a tap pick? A movable piece, its target square, the
 * player's pool icon (entering) or Home box (bearing off). */
static int move_at(int px, int py)
{
	int p = G.pos.turn;
	int row_y = p == 0 ? ROW_YOU : ROW_OPP;
	if (in_rect(px, py, 8, row_y, 120, 26)) {
		for (int i = 0; i < G.nmoves; i++)
			if (G.moves[i].from == 0)
				return i;
	}
	if (in_rect(px, py, HOME_X - 44, row_y, HOME_W + 48, 26)) {
		for (int i = 0; i < G.nmoves; i++)
			if (G.moves[i].to == UR_OFF)
				return i;
	}
	if (px < MAP_X || py < MAP_Y)
		return -1;
	int col = (px - MAP_X) / CELL, row = (py - MAP_Y) / CELL;
	if (col > 7 || row > 2)
		return -1;
	int step = ur_cell_step(p, row, col);
	if (step < 0)
		return -1;
	for (int i = 0; i < G.nmoves; i++)
		if (G.moves[i].from == step)
			return i;
	for (int i = 0; i < G.nmoves; i++)
		if (G.moves[i].to == step)
			return i;
	return -1;
}

static int pause_update(const Input *in)
{
	if (in->down & (KEY_START | KEY_B)) {
		G.paused = false;
		return PLAY_RUNNING;
	}
	if (in->down & KEY_DUP)
		G.pause_sel = (G.pause_sel + 2) % 3;
	if (in->down & KEY_DDOWN)
		G.pause_sel = (G.pause_sel + 1) % 3;
	int pick = -1;
	if (in->down & KEY_A)
		pick = G.pause_sel;
	if (in->touch_down)
		for (int i = 0; i < 3; i++)
			if (in_rect(in->tx, in->ty, 60, 70 + i * 46, 200, 38))
				pick = i;
	if (pick >= 0)
		sfx_play(SFX_TICK, 0, 1);
	if (pick == 0)
		G.paused = false;
	if (pick == 1)
		return PLAY_RULES;
	if (pick == 2) {
		G.paused = false;
		return PLAY_QUIT;
	}
	return PLAY_RUNNING;
}

int play_update(float dt, const Input *in)
{
	G.clock += dt;
	world_update(dt);
	if (G.paused)
		return pause_update(in);
	if ((in->down & KEY_START) && G.phase != PH_OVER) {
		G.paused = true;
		G.pause_sel = 0;
		sfx_play(SFX_TICK, 0, 1);
		return PLAY_RUNNING;
	}
	if (G.timer > 0)
		G.timer -= dt;

	switch (G.phase) {
	case PH_INTRO:
		if (G.timer <= 0)
			G.phase = PH_TURN;
		break;
	case PH_TURN:
		begin_turn();
		break;
	case PH_WAIT_ROLL:
		if ((in->down & KEY_A) || (in->touch_down && in_rect(in->tx, in->ty, 200, 186, 104, 46))) {
			sfx_play(SFX_TICK, 0, 0.8f);
			do_roll();
		}
		break;
	case PH_AI_PREROLL:
		if (G.timer <= 0)
			do_roll();
		break;
	case PH_ROLLING:
		if (!world_busy() && G.timer <= 0)
			after_roll();
		break;
	case PH_PICK: {
		int n = G.nmoves;
		if (in->down & (KEY_DRIGHT | KEY_DDOWN | KEY_R)) {
			G.sel = G.sel_by_pad ? (G.sel + 1) % n : 0;
			G.sel_by_pad = true;
			sfx_play(SFX_TICK, 0, 0.6f);
		}
		if (in->down & (KEY_DLEFT | KEY_DUP | KEY_L)) {
			G.sel = G.sel_by_pad ? (G.sel + n - 1) % n : n - 1;
			G.sel_by_pad = true;
			sfx_play(SFX_TICK, 0, 0.6f);
		}
		highlight();
		if (in->down & KEY_A) {
			execute(G.sel_by_pad ? G.sel : 0);
			break;
		}
		if (in->touch_down) {
			int i = move_at(in->tx, in->ty);
			if (i >= 0)
				execute(i);
		}
		break;
	}
	case PH_AUTO:
		highlight();
		if (G.timer <= 0)
			execute(0);
		break;
	case PH_AI_THINK: {
		UrMove m;
		UrAiStats st;
		if (G.timer <= 0 && ai_worker_poll(&m, &st, &G.ai_ms)) {
			G.ai_nodes = st.nodes;
			G.sel = 0;
			for (int i = 0; i < G.nmoves; i++)
				if (G.moves[i].from == m.from && G.moves[i].to == m.to)
					G.sel = i;
			G.phase = PH_AI_SHOW;
			G.timer = 0.4f;
			highlight();
		}
		break;
	}
	case PH_AI_SHOW:
		if (G.timer <= 0)
			execute(G.sel);
		break;
	case PH_MOVING:
		if (!world_busy())
			after_move();
		break;
	case PH_AFTER:
		if (G.timer <= 0)
			G.phase = PH_TURN;
		break;
	case PH_PASS:
		if (G.timer <= 0) {
			ur_pass(&G.pos);
			G.last.extra_turn = false;
			G.last.player = G.pos.turn ^ 1;
			G.msg[0] = 0;
			G.phase = PH_TURN;
		}
		break;
	case PH_OVER:
		G.over_t += dt;
		if (G.over_t < 1.0f)
			break;
		if (in->down & (KEY_DLEFT | KEY_DRIGHT))
			G.over_sel ^= 1;
		if (in->down & KEY_B)
			return PLAY_QUIT;
		if (in->down & KEY_A) {
			sfx_play(SFX_TICK, 0, 1);
			if (G.over_sel == 0)
				play_new(G.mode, G.level);
			else
				return PLAY_QUIT;
		}
		if (in->touch_down) {
			if (in_rect(in->tx, in->ty, 16, 180, 140, 44)) {
				sfx_play(SFX_TICK, 0, 1);
				play_new(G.mode, G.level);
			} else if (in_rect(in->tx, in->ty, 164, 180, 140, 44)) {
				sfx_play(SFX_TICK, 0, 1);
				return PLAY_QUIT;
			}
		}
		break;
	}
	return PLAY_RUNNING;
}

bool play_room_view(void)
{
	return G.phase == PH_OVER && G.over_t > 0.8f;
}

void play_debug(PlayDebug *d)
{
	d->phase = G.phase;
	d->nmoves = G.nmoves;
	d->turn = G.pos.turn;
	d->roll = G.roll;
	d->winner = G.winner;
	d->mode = G.mode;
	d->level = G.level;
	d->paused = G.paused;
	memcpy(d->moves, G.moves, sizeof(G.moves));
	d->msg = G.msg;
	d->sub = G.sub;
	d->pos = G.pos;
	d->ai_ms = G.ai_ms;
	d->ai_nodes = G.ai_nodes;
}

bool play_over(void)
{
	return G.phase == PH_OVER;
}

int play_winner(void)
{
	return G.winner;
}

/* ---- drawing ---- */

static void player_row(int y, int player)
{
	bool active = G.pos.turn == player && G.phase != PH_OVER && G.phase != PH_INTRO;
	spr(player == 0 ? art_pc_l_s_idx : art_pc_d_s_idx, 16, y + 3);
	const char *nm = name_of(player);
	text(TS_HEAD, 42, y + 13, A_LEFT, "%s", nm);
	if (active)
		draw_rect(42, y + 23, text_w(TS_HEAD, nm), 3, COL_ACTIVE);
	if (G.pos.pool[player] > 0 && G.pos.pool[player] < UR_PIECES)
		text(TS_BODY, 50 + text_w(TS_HEAD, nm), y + 13, A_LEFT, "%d waiting", G.pos.pool[player]);
	text(TS_BODY, HOME_X - 6, y + 13, A_RIGHT, "Home");
	panel(P_BOX3, HOME_X, y + 2, HOME_W, 22);
	text(TS_NUM, HOME_X + HOME_W / 2, y + 13, A_CENTER, "%d/7", G.pos.done[player]);
}

static void draw_pieces(void)
{
	for (int p = 0; p < 2; p++)
		for (int s = 1; s <= UR_TRACK; s++)
			if (G.pos.mask[p] & (1u << s)) {
				int row, col;
				ur_step_cell(p, s, &row, &col);
				spr(p == 0 ? art_pc_l_idx : art_pc_d_idx, cell_center_x(col) - 13, cell_center_y(row) - 13);
			}
}

static void draw_highlights(void)
{
	int p = G.pos.turn;
	bool pick = G.phase == PH_PICK || G.phase == PH_AUTO || G.phase == PH_AI_SHOW;
	if (!pick)
		return;
	int row_y = p == 0 ? ROW_YOU : ROW_OPP;
	for (int i = 0; i < G.nmoves; i++) {
		bool sel = (G.sel_by_pad || G.phase != PH_PICK) && i == G.sel;
		if (G.phase == PH_AI_SHOW && i != G.sel)
			continue;
		const UrMove *m = &G.moves[i];
		int row, col;
		if (m->from == 0)
			spr(sel ? art_ring_sel_idx : art_ring_idx, 26 - (sel ? 17 : 16), row_y + 13 - (sel ? 17 : 16));
		else if (ur_step_cell(p, m->from, &row, &col))
			spr(sel ? art_ring_sel_idx : art_ring_idx, cell_center_x(col) - (sel ? 17 : 16),
			    cell_center_y(row) - (sel ? 17 : 16));
		if (m->to == UR_OFF) {
			draw_rect(HOME_X - 2, row_y, HOME_W + 4, 2, COL_TARGET);
			draw_rect(HOME_X - 2, row_y + 24, HOME_W + 4, 2, COL_TARGET);
			draw_rect(HOME_X - 2, row_y, 2, 26, COL_TARGET);
			draw_rect(HOME_X + HOME_W, row_y, 2, 26, COL_TARGET);
		} else if (ur_step_cell(p, m->to, &row, &col)) {
			spr(sel ? art_tgt_sel_idx : art_tgt_idx, MAP_X + col * CELL + 1.5f, MAP_Y + row * CELL + 1.5f);
		}
	}
}

/* Message column on the left (x 16..176), dice or Roll button on the right. */
static void draw_action_row(void)
{
	text(TS_HEAD, 16, 196, A_LEFT, "%s", G.msg);
	if (G.sub[0])
		text_wrap(G.sub_quote ? TS_QUOTE : TS_BODY, 16, 214, 160, 13, G.sub);
	bool show_dice = G.roll >= 0 && G.phase != PH_WAIT_ROLL && G.phase != PH_AI_PREROLL && G.phase != PH_ROLLING;
	if (G.phase == PH_WAIT_ROLL) {
		spr_btn(art_btn_roll_idx, 200, 186);
	} else if (show_dice) {
		for (int i = 0; i < 4; i++)
			spr(G.marked[i] ? art_die_m_idx : art_die_u_idx, 182 + i * 21, 198);
		panel(P_BOX3, 272, 190, 32, 34);
		text(TS_BIG, 288, 207, A_CENTER, "%d", G.roll);
	}
}

static void draw_pause(void)
{
	draw_fade(0.9f, 320, 240, COL_SCRIM);
	text(TS_TITLE, 160, 40, A_CENTER, "Paused");
	static const char *const items[3] = { "Resume", "How to play", "Quit to title" };
	for (int i = 0; i < 3; i++) {
		float y = 70 + i * 46;
		panel_btn(P_TILE6, 60, y, 200, 38);
		if (i == G.pause_sel) {
			draw_rect(60, y + 38, 200, 3, COL_ACCENT);
		}
		text(TS_TILE, 160, y + 19 + (touched_recently(60, y, 200, 38) ? 1 : 0), A_CENTER, "%s", items[i]);
	}
	text(TS_BODY, 160, 224, A_CENTER, "Your game is saved");
}

static void draw_over(void)
{
	draw_fade(0.78f, 320, 240, COL_SCRIM);
	bool you = G.mode == MODE_AI && G.winner == 0;
	char head[32];
	if (G.mode == MODE_AI)
		snprintf(head, sizeof(head), you ? "You win!" : "%s wins", level_name[G.level]);
	else
		snprintf(head, sizeof(head), "%s wins!", name_of(G.winner));
	text(TS_HUGE, 160, 52, A_CENTER, "%s", head);
	spr(art_rosette_big_idx, 160 - 24, 78);
	if (G.mode == MODE_AI)
		text(TS_TEXT, 160, 146, A_CENTER, "Record vs %s:  %d won,  %d lost", level_name[G.level],
		     settings.wins[G.level], settings.losses[G.level]);
	else
		text(TS_TEXT, 160, 146, A_CENTER, "Home: %d to %d", G.pos.done[0], G.pos.done[1]);
	if (G.over_t < 1.0f)
		return;
	static const char *const items[2] = { "Play again", "Title" };
	for (int i = 0; i < 2; i++) {
		float x = 16 + i * 148;
		panel_btn(i == 0 ? P_BTN3 : P_TILE6, x, 180, 140, 44);
		if (i == G.over_sel)
			draw_rect(x, 224, 140, 3, COL_P_SHELL_HI);
		if (i == 0)
			text(TS_BTN, x + 70, 202 + (touched_recently(x, 180, 140, 44) ? 1 : 0), A_CENTER, "%s", items[i]);
		else
			text(TS_TILE, x + 70, 202 + (touched_recently(x, 180, 140, 44) ? 1 : 0), A_CENTER, "%s", items[i]);
	}
}

void play_draw_bottom(void)
{
	draw_rect(0, 0, 320, 240, COL_GROUND);
	player_row(ROW_OPP, 1);
	spr(art_map_idx, MAP_X - 3, MAP_Y - 3);
	draw_pieces();
	draw_highlights();
	player_row(ROW_YOU, 0);
	draw_action_row();
	if (G.phase == PH_OVER)
		draw_over();
	if (G.paused)
		draw_pause();
}

void play_draw_top(void)
{
	if (G.phase == PH_OVER && G.over_t > 1.0f) {
		bool you = G.mode == MODE_AI && G.winner == 0;
		char head[32];
		if (G.mode == MODE_AI)
			snprintf(head, sizeof(head), you ? "You win!" : "%s wins", level_name[G.level]);
		else
			snprintf(head, sizeof(head), "%s wins!", name_of(G.winner));
		float w = text_w(TS_HUGE, head) + 48;
		panel(P_SCRIM6, 200 - w / 2, 14, w, 52);
		text(TS_HUGE, 200, 40, A_CENTER, "%s", head);
	}
}
