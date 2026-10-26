/* The Royal Game of Ur for the 3DS.
 *
 * Top screen: Frank's 3D board in stereo (room view in the menus, straight
 * top-down during play). Bottom screen: menus and the touch map.
 *
 * Debug: SELECT saves both screens to sdmc:/3ds/ur-3ds/shots/,
 * SELECT+R toggles a frame-time readout. */
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ai_worker.h"
#include "draw.h"
#include "pages.h"
#include "play.h"
#include "save.h"
#include "scene.h"
#include "sfx.h"
#include "shot.h"
#include "world.h"

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	 GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	 GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define CLEAR_TOP 0x1A1410FF
#define TICKS_PER_MS (SYSCLOCK_ARM11 / 1000.0)

enum { FMT_RGB565, FMT_ETC1, FMT_COUNT };
static const char *fmt_file[FMT_COUNT] = { "rgb565", "etc1" };
static C3D_Tex tex[SCENE_MAX_TEX][FMT_COUNT];

/* ---- cameras ---- */

#define BOARD_TOP 0.024f
#define TOP_FOVY 30.0f
/* camera height that fits each board plus a margin across the 400 px width */
static const float top_height[UR_BOARD_COUNT] = { 0.37f, 0.55f };
#define ROOM_FOVY 35.224f

typedef struct {
	C3D_FVec eye, target, up;
	float fovy;
} Pose;

static Pose pose_top(void)
{
	float h = top_height[world_board()];
	Pose p = { FVec3_New(0, BOARD_TOP + h, 0), FVec3_New(0, BOARD_TOP, 0), FVec3_New(0, 0, -1), TOP_FOVY };
	return p;
}

/* camera.json orbit, drifting slowly so the room feels alive */
static Pose pose_room(float t)
{
	float yaw = C3D_AngleFromDegrees(17.354f + 14.0f * sinf(t * 0.13f));
	float pitch = C3D_AngleFromDegrees(35.0f + 3.0f * sinf(t * 0.09f));
	float dist = 0.5806f;
	C3D_FVec c = FVec3_New(0, 0.075f, -0.02f);
	Pose p = { FVec3_New(c.x + dist * cosf(pitch) * sinf(yaw), c.y + dist * sinf(pitch),
	                     c.z + dist * cosf(pitch) * cosf(yaw)),
	           c, FVec3_New(0, 1, 0), ROOM_FOVY };
	return p;
}

static C3D_FVec vlerp(C3D_FVec a, C3D_FVec b, float t)
{
	return FVec3_Add(a, FVec3_Scale(FVec3_Subtract(b, a), t));
}

static Pose pose_lerp(const Pose *a, const Pose *b, float t)
{
	t = t * t * (3 - 2 * t);
	Pose p = { vlerp(a->eye, b->eye, t), vlerp(a->target, b->target, t), FVec3_Normalize(vlerp(a->up, b->up, t)),
	           a->fovy + (b->fovy - a->fovy) * t };
	return p;
}

/* ---- screens ---- */

enum { SCR_TITLE, SCR_LEVEL, SCR_PLAY, SCR_PAGES, SCR_SETTINGS, SCR_SPLASH };
static int screen = SCR_SPLASH, pages_return = SCR_TITLE;
static int menu_sel = 1, level_sel, set_sel, board_sel;
static PlayMode new_mode = MODE_AI;
static bool has_save;

typedef struct {
	const char *label, *sub;
} Tile;

static const Tile title_tiles[6] = {
	{ "Continue", "Resume your game" }, { "Play", "vs the computer" },
	{ "Two players", "Share one 3DS" }, { "How to play", "Rules and controls" },
	{ "The story", "4,500 years of Ur" }, { "Settings", "Sound and play" },
};
static const char *const level_name[3] = { "Beginner", "Medium", "Expert" };

static void tile(float x, float y, float w, float h, const char *label, const char *sub, bool sel, bool dim)
{
	float dy = touched_recently(x, y, w, h) ? 1 : 0;
	panel(P_TILE6, x, y + dy, w, h);
	text(TS_TILE, x + 14, y + dy + h / 2 - (sub ? 7 : 0), A_LEFT, "%s", label);
	if (sub)
		text(TS_TILE_SOFT, x + 14, y + dy + h / 2 + 9, A_LEFT, "%s", sub);
	if (dim)
		draw_rect(x, y + dy, w, h, C2D_Color32(24, 33, 58, 150));
	if (sel)
		draw_rect(x, y + h, w, 3, COL_ACCENT);
}

static void title_draw(void)
{
	draw_rect(0, 0, 320, 240, COL_GROUND);
	spr(art_rosette_small_idx, 160 - 12, 10);
	for (int i = 0; i < 6; i++) {
		float x = 16 + (i % 2) * 148, y = 44 + (i / 2) * 60;
		tile(x, y, 140, 50, title_tiles[i].label, title_tiles[i].sub, i == menu_sel, i == 0 && !has_save);
	}
	text(TS_BODY, 160, 226, A_CENTER, "Board after the British Museum's Ur board");
}

/* New game: pick the board, then the opponent (or Start for two players). */
static const char *const board_label[UR_BOARD_COUNT] = { "Ur", "Twenty Squares" };

static void level_draw(void)
{
	draw_rect(0, 0, 320, 240, COL_GROUND);
	text(TS_TITLE, 160, 16, A_CENTER, "%s", new_mode == MODE_AI ? "New game" : "Two players");
	for (int b = 0; b < UR_BOARD_COUNT; b++) {
		float x = 16 + b * 148, y = 30;
		float dy = touched_recently(x, y, 140, 62) ? 1 : 0;
		panel(P_TILE6, x, y + dy, 140, 62);
		size_t icon = b == UR_BOARD_UR ? art_icon_ur_idx : art_icon_twenty_idx;
		spr(icon, x + 70 - spr_w(icon) / 2.0f, y + dy + 7);
		text(TS_TILE, x + 70, y + dy + 50, A_CENTER, "%s", board_label[b]);
		if (b == board_sel)
			draw_rect(x, y + 62, 140, 3, COL_ACCENT);
	}
	if (new_mode == MODE_AI) {
		for (int i = 0; i < 3; i++) {
			float y = 102 + i * 34;
			tile(16, y, 288, 30, level_name[i], NULL, i == level_sel, false);
			text(TS_TILE_SOFT, 290, y + 15, A_RIGHT, "%d won  %d lost", settings.wins[board_sel][i],
			     settings.losses[board_sel][i]);
		}
	} else {
		panel_btn(P_BTN3, 90, 118, 140, 44);
		text(TS_BTN, 160, 140 + (touched_recently(90, 118, 140, 44) ? 1 : 0), A_CENTER, "Start");
	}
	panel_btn(P_TILE3, 16, 204, 80, 28);
	text(TS_TILE, 56, 218, A_CENTER, "Back");
}

static void settings_draw(void)
{
	static const char *const names[3] = { "Sound effects", "Music", "Auto-move" };
	static const char *const subs[3] = { "Dice, pieces and chimes", "Lyre and drum", "Plays your only legal move" };
	bool vals[3] = { settings.sound, settings.music, settings.auto_move };
	draw_rect(0, 0, 320, 240, COL_GROUND);
	text(TS_TITLE, 160, 22, A_CENTER, "Settings");
	for (int i = 0; i < 3; i++) {
		float y = 44 + i * 52;
		tile(16, y, 288, 44, names[i], subs[i], i == set_sel, false);
		panel(vals[i] ? P_BTN3 : P_BOX3, 244, y + 10, 48, 24);
		text(vals[i] ? TS_BTN : TS_NUM, 268, y + 22, A_CENTER, "%s", vals[i] ? "On" : "Off");
	}
	panel_btn(P_TILE3, 16, 200, 80, 30);
	text(TS_TILE, 56, 215, A_CENTER, "Back");
}

/* ---- boot splash: painted key art on top, title and copyright below ---- */

static float splash_t, leave_t = -1, fade_in;
static C2D_SpriteSheet keyart;

static void splash_update(const Input *in, float dt)
{
	splash_t += dt;
	if (leave_t >= 0) {
		leave_t += dt;
		if (leave_t > 0.35f) {
			screen = SCR_TITLE;
			fade_in = 1;
			leave_t = -1;
		}
		return;
	}
	if (splash_t > 0.8f && ((in->down & (KEY_A | KEY_START)) || in->touch_down)) {
		sfx_play(SFX_TICK, 0, 1);
		leave_t = 0;
	}
}

static float splash_black(void)
{
	float f = 1.0f - splash_t / 1.2f;
	if (leave_t >= 0)
		f = leave_t / 0.35f;
	return f;
}

/* A slow pan and zoom across the 640x384 painting (Ken Burns). */
static void splash_draw_top(void)
{
	if (!keyart)
		return;
	C2D_Image img = C2D_SpriteSheetGetImage(keyart, 0);
	const Tex3DS_SubTexture *ps = img.subtex;
	float e = splash_t / 14.0f;
	e = e > 1 ? 1 : e * (2 - e);
	float w = 640 - 110 * e, h = w * 0.6f;
	float cx = 320 + 40 * e, cy = 192 + 18 * e;
	float x0 = cx - w / 2, y0 = cy - h / 2;
	Tex3DS_SubTexture sub;
	sub.width = (u16)w;
	sub.height = (u16)h;
	sub.left = ps->left + (ps->right - ps->left) * x0 / 640.0f;
	sub.right = ps->left + (ps->right - ps->left) * (x0 + w) / 640.0f;
	sub.top = ps->top + (ps->bottom - ps->top) * y0 / 384.0f;
	sub.bottom = ps->top + (ps->bottom - ps->top) * (y0 + h) / 384.0f;
	img.subtex = &sub;
	C2D_DrawImageAt(img, 0, 0, 0.5f, NULL, 400.0f / w, 240.0f / h);
	draw_fade(splash_black(), 400, 240, C2D_Color32(0, 0, 0, 255));
}

static void splash_draw_bottom(void)
{
	draw_rect(0, 0, 320, 240, COL_GROUND);
	float u = (splash_t - 0.5f) / 0.8f;
	u = u < 0 ? 0 : u > 1 ? 1 : u;
	float back = 1 + 2.2f * (u - 1) * (u - 1) * (u - 1) + 1.2f * (u - 1) * (u - 1); /* ease-out-back */
	spr(art_logo_idx, 10, -90 + 130 * back);
	if (splash_t > 1.6f && fmodf(splash_t - 1.6f, 1.6f) < 1.25f)
		text(TS_HEAD, 160, 170, A_CENTER, "Tap to begin");
	text(TS_BODY, 160, 226, A_CENTER, "\u00a9 2026 No Banners Software");
	draw_fade(splash_black(), 320, 240, C2D_Color32(0, 0, 0, 255));
}


static void title_update(const Input *in)
{
	int pick = -1;
	if ((in->down & KEY_DRIGHT) && menu_sel % 2 == 0)
		menu_sel++;
	if ((in->down & KEY_DLEFT) && menu_sel % 2 == 1)
		menu_sel--;
	if ((in->down & KEY_DDOWN) && menu_sel + 2 < 6)
		menu_sel += 2;
	if ((in->down & KEY_DUP) && menu_sel - 2 >= 0)
		menu_sel -= 2;
	if (in->down & KEY_A)
		pick = menu_sel;
	if (in->touch_down)
		for (int i = 0; i < 6; i++)
			if (in_rect(in->tx, in->ty, 16 + (i % 2) * 148, 44 + (i / 2) * 60, 140, 50))
				pick = menu_sel = i;
	if (pick < 0 || (pick == 0 && !has_save))
		return;
	sfx_play(SFX_TICK, 0, 1);
	switch (pick) {
	case 0:
		if (play_resume())
			screen = SCR_PLAY;
		break;
	case 1:
	case 2:
		new_mode = pick == 1 ? MODE_AI : MODE_2P;
		level_sel = settings.last_level;
		board_sel = settings.last_board;
		screen = SCR_LEVEL;
		break;
	case 3:
	case 4:
		pages_open(pick == 3 ? PAGES_RULES : PAGES_STORY);
		pages_return = SCR_TITLE;
		screen = SCR_PAGES;
		break;
	case 5:
		set_sel = 0;
		screen = SCR_SETTINGS;
		break;
	}
}

static void level_update(const Input *in)
{
	if (in->down & (KEY_DLEFT | KEY_DRIGHT | KEY_L | KEY_R)) {
		board_sel ^= 1;
		sfx_play(SFX_TICK, 0, 0.6f);
	}
	if (new_mode == MODE_AI) {
		if (in->down & KEY_DUP)
			level_sel = (level_sel + 2) % 3;
		if (in->down & KEY_DDOWN)
			level_sel = (level_sel + 1) % 3;
	}
	int pick = -1;
	bool back = in->down & KEY_B;
	if (in->down & KEY_A)
		pick = new_mode == MODE_AI ? level_sel : 0;
	if (in->touch_down) {
		for (int b = 0; b < UR_BOARD_COUNT; b++)
			if (in_rect(in->tx, in->ty, 16 + b * 148, 30, 140, 62)) {
				board_sel = b;
				sfx_play(SFX_TICK, 0, 0.6f);
			}
		if (new_mode == MODE_AI) {
			for (int i = 0; i < 3; i++)
				if (in_rect(in->tx, in->ty, 16, 102 + i * 34, 288, 30))
					pick = level_sel = i;
		} else if (in_rect(in->tx, in->ty, 90, 118, 140, 44)) {
			pick = 0;
		}
		back |= in_rect(in->tx, in->ty, 16, 204, 80, 28);
	}
	if (back) {
		sfx_play(SFX_TICK, 0, 1);
		screen = SCR_TITLE;
	} else if (pick >= 0) {
		sfx_play(SFX_TICK, 0, 1);
		play_new(new_mode, (UrLevel)(new_mode == MODE_AI ? pick : UR_MEDIUM), board_sel);
		screen = SCR_PLAY;
	}
}

static void settings_update(const Input *in)
{
	if (in->down & KEY_DUP)
		set_sel = (set_sel + 2) % 3;
	if (in->down & KEY_DDOWN)
		set_sel = (set_sel + 1) % 3;
	int pick = -1;
	bool back = in->down & KEY_B;
	if (in->down & KEY_A)
		pick = set_sel;
	if (in->touch_down) {
		for (int i = 0; i < 3; i++)
			if (in_rect(in->tx, in->ty, 16, 44 + i * 52, 288, 44))
				pick = set_sel = i;
		back |= in_rect(in->tx, in->ty, 16, 200, 80, 30);
	}
	if (back) {
		play_settings_save();
		sfx_play(SFX_TICK, 0, 1);
		screen = SCR_TITLE;
		return;
	}
	if (pick == 0)
		settings.sound = !settings.sound;
	if (pick == 1)
		settings.music = !settings.music;
	if (pick == 2)
		settings.auto_move = !settings.auto_move;
	if (pick >= 0) {
		sfx_enable(settings.sound, settings.music);
		sfx_play(SFX_TICK, 0, 1);
	}
}

#ifdef UR_ROBOT
#include "robot.h"
#endif

/* ---- main ---- */

int main(void)
{
	gfxInitDefault();
	gfxSet3D(true);
	romfsInit();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 2);
	C2D_Prepare();

	C3D_RenderTarget *left = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget *right = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(left, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(right, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	Scene scene;
	char err[128] = "";
	bool ok = render_init() && scene_load(&scene, "romfs:/scene.bin");
	if (!ok)
		snprintf(err, sizeof(err), "scene.bin failed");
	/* only the RGB565 set is loaded; ETC1 stays available for a later memory squeeze */
	for (int t = 0; ok && t < scene.tex_count; t++) {
		char path[96];
		snprintf(path, sizeof(path), "romfs:/tex/%s_%s.t3x", scene.tex_names[t], fmt_file[FMT_RGB565]);
		if (!tex_load(&tex[t][FMT_RGB565], path)) {
			ok = false;
			snprintf(err, sizeof(err), "%s failed", path);
		}
	}
	if (ok && !world_init(&scene)) {
		ok = false;
		snprintf(err, sizeof(err), "world init failed");
	}
	keyart = C2D_SpriteSheetLoad("romfs:/gfx/keyart.t3x");
	if (ok && draw_init() != 0) {
		ok = false;
		snprintf(err, sizeof(err), "art or fonts failed");
	}
	ai_worker_init();
	play_init();
	sfx_init();
	sfx_enable(settings.sound, settings.music);
	has_save = play_has_save();
	menu_sel = has_save ? 0 : 1;

	srand(svcGetSystemTick());
	UrPos start;
	ur_init(&start, 0, UR_BOARD_UR);
	world_reset(&start);

	C2D_TextBuf dbg_buf = C2D_TextBufNew(512);
	bool show_dbg = false;
	float blend = 1, clock = 0;
	u64 last = svcGetSystemTick();
	double frame_ms = 16.7, worst_ms = 0;
	int shot_n = 0;

	while (aptMainLoop()) {
		u64 now = svcGetSystemTick();
		double ms = (now - last) / TICKS_PER_MS;
		float dt = (float)(ms / 1000.0);
		frame_ms = frame_ms * 0.95 + ms * 0.05;
		if (ms > worst_ms)
			worst_ms = ms;
		last = now;
		if (dt > 0.05f)
			dt = 0.05f;
		clock += dt;

		hidScanInput();
		Input in;
		memset(&in, 0, sizeof(in));
		in.down = hidKeysDown();
		in.held = hidKeysHeld();
		if (in.down & KEY_TOUCH) {
			touchPosition tp;
			hidTouchRead(&tp);
			in.touch_down = true;
			in.tx = tp.px;
			in.ty = tp.py;
		}
#ifdef UR_ROBOT
		robot_input(&in, screen, dt);
		dt *= ROBOT_SPEED;
#endif
		if (in.touch_down)
			draw_touch(in.tx, in.ty);
		if ((in.held & KEY_SELECT) && (in.down & KEY_R)) {
			show_dbg = !show_dbg;
			worst_ms = 0;
		}
		bool want_shot = (in.down & KEY_SELECT) && !(in.held & KEY_R);
		if (in.held & KEY_SELECT)
			in.down &= ~(KEY_R | KEY_SELECT);

		if (!ok) {
			if (in.down & KEY_START)
				break;
		} else {
			switch (screen) {
			case SCR_SPLASH:
				world_update(dt);
				splash_update(&in, dt);
				break;
			case SCR_TITLE:
				world_update(dt);
				title_update(&in);
				break;
			case SCR_LEVEL:
				world_update(dt);
				level_update(&in);
				break;
			case SCR_SETTINGS:
				world_update(dt);
				settings_update(&in);
				break;
			case SCR_PAGES:
				world_update(dt);
				if (pages_update(in.down, in.touch_down, in.tx, in.ty)) {
					sfx_play(SFX_TICK, 0, 1);
					screen = pages_return;
				}
				break;
			case SCR_PLAY: {
				int r = play_update(dt, &in);
				if (r == PLAY_QUIT) {
					screen = SCR_TITLE;
					has_save = play_has_save();
					menu_sel = has_save ? 0 : 1;
				} else if (r == PLAY_RULES) {
					pages_open(PAGES_RULES);
					pages_return = SCR_PLAY;
					screen = SCR_PAGES;
				}
				break;
			}
			}
		}

		bool room = screen != SCR_PLAY && !(screen == SCR_PAGES && pages_return == SCR_PLAY);
		if (screen == SCR_PLAY && play_room_view())
			room = true;
		blend += (room ? 1.0f : -1.0f) * dt / 0.9f;
		blend = blend < 0 ? 0 : blend > 1 ? 1 : blend;

		Pose top = pose_top(), rv = pose_room(clock);
		Pose cam = pose_lerp(&top, &rv, blend);
		float focus = FVec3_Distance(cam.eye, cam.target);
		float slider = osGet3DSliderState();
		float iod = slider * focus / 7.0f;
		C3D_Mtx view, proj;
		Mtx_LookAt(&view, cam.eye, cam.target, cam.up, false);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(left, C3D_CLEAR_ALL, CLEAR_TOP, 0);
		C3D_RenderTargetClear(right, C3D_CLEAR_ALL, CLEAR_TOP, 0);
		for (int eye = 0; eye < 2 && ok; eye++) {
			if (eye == 1 && iod <= 0)
				break;
			C3D_RenderTarget *t = eye == 0 ? left : right;
			render_bind();
			C3D_FrameDrawOn(t);
			Mtx_PerspStereoTilt(&proj, C3D_AngleFromDegrees(cam.fovy), C3D_AspectRatioTop, 0.01f, 2.5f,
			                    eye == 0 ? -iod : iod, focus, false);
			render_camera(&proj, &view);
			if (screen != SCR_SPLASH)
				world_draw(FMT_RGB565, tex, blend > 0.5f);

			C2D_Prepare();
			C2D_SceneBegin(t);
			if (screen == SCR_SPLASH)
				splash_draw_top();
			else if (screen != SCR_PLAY && blend > 0.6f)
				spr(art_logo_idx, 50, 6);
			if (fade_in > 0)
				draw_fade(fade_in, 400, 240, C2D_Color32(0, 0, 0, 255));
			if (screen == SCR_PLAY)
				play_draw_top();
			if (show_dbg) {
				char line[160];
				snprintf(line, sizeof(line), "frame %.1f ms  worst %.1f  cpu %.1f gpu %.1f", frame_ms, worst_ms,
				         C3D_GetProcessingTime(), C3D_GetDrawingTime());
				C2D_TextBufClear(dbg_buf);
				C2D_Text tx;
				C2D_TextParse(&tx, dbg_buf, line);
				C2D_DrawRectSolid(0, 226, 0.5f, 400, 14, C2D_Color32(0, 0, 0, 170));
				C2D_DrawText(&tx, C2D_WithColor, 4, 227, 0.5f, 0.4f, 0.4f, C2D_Color32(255, 255, 255, 255));
			}
			C2D_Flush();
		}

		C2D_Prepare();
		C2D_TargetClear(bottom, COL_GROUND);
		C2D_SceneBegin(bottom);
		if (!ok) {
			C2D_TextBufClear(dbg_buf);
			C2D_Text tx;
			C2D_TextParse(&tx, dbg_buf, err);
			C2D_DrawText(&tx, C2D_WithColor, 8, 8, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
		} else {
			switch (screen) {
			case SCR_SPLASH: splash_draw_bottom(); break;
			case SCR_TITLE: title_draw(); break;
			case SCR_LEVEL: level_draw(); break;
			case SCR_SETTINGS: settings_draw(); break;
			case SCR_PAGES: pages_draw(); break;
			case SCR_PLAY: play_draw_bottom(); break;
			}
			if (fade_in > 0)
				draw_fade(fade_in, 320, 240, C2D_Color32(0, 0, 0, 255));
		}
		C3D_FrameEnd(0);
		if (fade_in > 0)
			fade_in -= dt / 0.5f;

		if (want_shot) {
			char name[32];
			snprintf(name, sizeof(name), "shot%03d", shot_n++);
			shot_save(name);
		}
#ifdef UR_ROBOT
		robot_after_frame(screen);
#endif
	}

	C2D_TextBufDelete(dbg_buf);
	if (keyart)
		C2D_SpriteSheetFree(keyart);
	ai_worker_exit();
	sfx_exit();
	draw_exit();
	world_exit();
	for (int t = 0; t < SCENE_MAX_TEX; t++)
		for (int f = 0; f < FMT_COUNT; f++)
			if (tex[t][f].data)
				C3D_TexDelete(&tex[t][f]);
	if (ok)
		scene_free(&scene);
	render_exit();
	C2D_Fini();
	C3D_Fini();
	romfsExit();
	gfxExit();
	return 0;
}
