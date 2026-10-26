/* M0 stereo test cart: Frank's asset pack in stereo on the top screen,
 * with a perf readout on the bottom screen.
 *
 * D-pad up/down switches the top-down play view and the angled room view.
 * L/R zoom, circle pad orbits (room view), A texture format, X load
 * multiplier, B back-face culling, Y timed bench (writes
 * sdmc:/ur-3ds-bench.txt), SELECT reset camera, START quit. */
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene.h"

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	 GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	 GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define CLEAR_TOP 0x2B1F17FF /* warm dark brown until the backdrop exists */
#define TICKS_PER_MS (SYSCLOCK_ARM11 / 1000.0)

enum { FMT_RGB565, FMT_ETC1, FMT_COUNT };
static const char *fmt_name[FMT_COUNT] = { "RGB565", "ETC1" };
static const char *fmt_file[FMT_COUNT] = { "rgb565", "etc1" };
static C3D_Tex tex[SCENE_MAX_TEX][FMT_COUNT];

static const int loads[] = { 1, 2, 4, 8, 16 };
#define LOAD_COUNT (int)(sizeof(loads) / sizeof(loads[0]))

/* Play view: straight down on the board, its length across the screen,
 * light player's row at the bottom. Height chosen so the board plus a margin
 * fills the 400 px width. */
#define BOARD_TOP 0.024f
#define TOP_FOVY 30.0f
#define TOP_HEIGHT 0.37f

/* Room view: the asset pack's camera.json orbit around (0, 0.006, 0). */
typedef struct {
	float yaw, pitch, dist;
} Orbit;
static const Orbit room_default = { 17.354f, 54.728f, 0.5806f };
#define ROOM_FOVY 35.224f
#define YAW_RANGE 30.0f
#define PITCH_MIN 35.0f
#define PITCH_MAX 70.0f

typedef struct {
	C3D_FVec eye, target, up;
	float fovy;
} Pose;

static Pose pose_top(float h)
{
	Pose p = { FVec3_New(0, BOARD_TOP + h, 0), FVec3_New(0, BOARD_TOP, 0), FVec3_New(0, 0, -1), TOP_FOVY };
	return p;
}

static Pose pose_room(const Orbit *o)
{
	float yaw = C3D_AngleFromDegrees(o->yaw), pitch = C3D_AngleFromDegrees(o->pitch);
	C3D_FVec t = FVec3_New(0, 0.006f, 0);
	Pose p = { FVec3_New(t.x + o->dist * cosf(pitch) * sinf(yaw), t.y + o->dist * sinf(pitch),
	                     t.z + o->dist * cosf(pitch) * cosf(yaw)),
	           t, FVec3_New(0, 1, 0), ROOM_FOVY };
	return p;
}

static C3D_FVec vlerp(C3D_FVec a, C3D_FVec b, float t)
{
	return FVec3_Add(a, FVec3_Scale(FVec3_Subtract(b, a), t));
}

static Pose pose_lerp(const Pose *a, const Pose *b, float t)
{
	t = t * t * (3 - 2 * t);
	Pose p = { vlerp(a->eye, b->eye, t), vlerp(a->target, b->target, t),
	           FVec3_Normalize(vlerp(a->up, b->up, t)), a->fovy + (b->fovy - a->fovy) * t };
	return p;
}

typedef struct {
	u64 last_tick;
	double frame_ms[60];
	int n, head;
	double cpu_ms, gpu_ms;
} Stats;

static void stats_frame(Stats *st)
{
	u64 now = svcGetSystemTick();
	if (st->last_tick) {
		st->frame_ms[st->head] = (now - st->last_tick) / TICKS_PER_MS;
		st->head = (st->head + 1) % 60;
		if (st->n < 60)
			st->n++;
	}
	st->last_tick = now;
	st->cpu_ms = C3D_GetProcessingTime();
	st->gpu_ms = C3D_GetDrawingTime();
}

static void stats_summary(const Stats *st, double *avg, double *worst)
{
	double sum = 0, mx = 0;
	for (int i = 0; i < st->n; i++) {
		sum += st->frame_ms[i];
		if (st->frame_ms[i] > mx)
			mx = st->frame_ms[i];
	}
	*avg = st->n ? sum / st->n : 0;
	*worst = mx;
}


static void draw_scene(const Scene *s, int fmt, int load)
{
	static const float white[4] = { 1, 1, 1, 1 };
	for (int k = 0; k < load; k++)
		for (int i = 0; i < s->node_count; i++) {
			const Node *n = &s->nodes[i];
			const Mesh *m = &s->meshes[n->mesh];
			render_mesh(m, &n->model, &tex[m->material][fmt], white);
		}
}

int main(void)
{
	gfxInitDefault();
	gfxSet3D(true);
	romfsInit();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

	C3D_RenderTarget *left = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget *right = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(left, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(right, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	C2D_TextBuf textbuf = C2D_TextBufNew(4096);
	char line[1024], err[256] = "";

	Scene scene;
	bool ok = render_init();
	if (!ok)
		snprintf(err, sizeof(err), "shader load failed");
	if (ok && !scene_load(&scene, "romfs:/scene.bin")) {
		ok = false;
		snprintf(err, sizeof(err), "romfs:/scene.bin failed");
	}
	for (int t = 0; ok && t < scene.tex_count; t++)
		for (int f = 0; f < FMT_COUNT; f++) {
			char path[96];
			snprintf(path, sizeof(path), "romfs:/tex/%s_%s.t3x", scene.tex_names[t], fmt_file[f]);
			if (!tex_load(&tex[t][f], path)) {
				ok = false;
				snprintf(err, sizeof(err), "%s failed", path);
			}
		}

	Orbit room = room_default;
	float top_h = TOP_HEIGHT;
	bool room_view = false;
	float blend = 0; /* 0 = top-down, 1 = room */
	int fmt = FMT_RGB565, load_i = 0;
	bool cull = true;
	Stats st = { 0 };

	/* Timed bench: 4 s per load level at the default camera, logged to SD. */
	bool bench = false;
	int bench_i = 0;
	u64 bench_start = 0;
	char bench_log[1024] = "";
#ifdef UR_AUTOBENCH
	bench = true;
	bench_start = svcGetSystemTick();
#endif

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown(), held = hidKeysHeld();
		if (down & KEY_START)
			break;
		if (down & KEY_A)
			fmt = (fmt + 1) % FMT_COUNT;
		if (down & KEY_X)
			load_i = (load_i + 1) % LOAD_COUNT;
		if (down & KEY_B)
			cull = !cull;
		if (down & KEY_SELECT) {
			room = room_default;
			top_h = TOP_HEIGHT;
		}
		if (down & KEY_DUP)
			room_view = true;
		if (down & KEY_DDOWN)
			room_view = false;
		if ((down & KEY_Y) && !bench) {
			bench = true;
			bench_i = 0;
			bench_log[0] = 0;
			bench_start = svcGetSystemTick();
		}

		circlePosition cp;
		hidCircleRead(&cp);
		if (room_view) {
			if (abs(cp.dx) > 15)
				room.yaw += cp.dx / 156.0f * 1.5f;
			if (abs(cp.dy) > 15)
				room.pitch -= cp.dy / 156.0f * 1.0f;
			if (held & KEY_L)
				room.dist *= 1.01f;
			if (held & KEY_R)
				room.dist /= 1.01f;
		} else {
			if (held & KEY_L)
				top_h *= 1.01f;
			if (held & KEY_R)
				top_h /= 1.01f;
		}
		float y0 = room_default.yaw;
		room.yaw = room.yaw < y0 - YAW_RANGE ? y0 - YAW_RANGE : room.yaw > y0 + YAW_RANGE ? y0 + YAW_RANGE : room.yaw;
		room.pitch = room.pitch < PITCH_MIN ? PITCH_MIN : room.pitch > PITCH_MAX ? PITCH_MAX : room.pitch;
		room.dist = room.dist < 0.25f ? 0.25f : room.dist > 0.70f ? 0.70f : room.dist;
		top_h = top_h < 0.20f ? 0.20f : top_h > 0.60f ? 0.60f : top_h;
		blend += room_view ? 1.0f / 50 : -1.0f / 50; /* ~0.8 s glide */
		blend = blend < 0 ? 0 : blend > 1 ? 1 : blend;

		if (bench) {
			room_view = false;
			blend = 0;
			top_h = TOP_HEIGHT;
			load_i = bench_i;
			double secs = (svcGetSystemTick() - bench_start) / (TICKS_PER_MS * 1000.0);
			if (secs > 4.0) {
				double avg, worst;
				stats_summary(&st, &avg, &worst);
				char row[96];
				snprintf(row, sizeof(row), "%s x%-2d %5d tris/eye  avg %5.2f ms  worst %5.2f ms  cpu %4.1f gpu %4.1f\n",
				         fmt_name[fmt], loads[bench_i], scene.tris * loads[bench_i], avg, worst, st.cpu_ms, st.gpu_ms);
				strncat(bench_log, row, sizeof(bench_log) - strlen(bench_log) - 1);
				bench_i++;
				bench_start = svcGetSystemTick();
				st.n = 0;
				if (bench_i >= LOAD_COUNT) {
					bench = false;
					load_i = 0;
					FILE *f = fopen("sdmc:/ur-3ds-bench.txt", "a");
					if (f) {
						fprintf(f, "--- 3D slider %.2f, cull %s\n%s", osGet3DSliderState(), cull ? "on" : "off", bench_log);
						fclose(f);
					}
				}
			}
		}

		float slider = osGet3DSliderState();
		Pose top = pose_top(top_h), rv = pose_room(&room);
		Pose cam = pose_lerp(&top, &rv, blend);
		float focus = FVec3_Distance(cam.eye, cam.target);
		float iod = slider * focus / 6.0f;
		C3D_Mtx view, proj;
		Mtx_LookAt(&view, cam.eye, cam.target, cam.up, false);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(left, C3D_CLEAR_ALL, CLEAR_TOP, 0);
		C3D_RenderTargetClear(right, C3D_CLEAR_ALL, CLEAR_TOP, 0);
		if (ok) {
			render_bind();
			C3D_CullFace(cull ? GPU_CULL_BACK_CCW : GPU_CULL_NONE);

			C3D_FrameDrawOn(left);
			Mtx_PerspStereoTilt(&proj, C3D_AngleFromDegrees(cam.fovy), C3D_AspectRatioTop, 0.01f, 2.5f, -iod, focus, false);
			render_camera(&proj, &view);
			draw_scene(&scene, fmt, loads[load_i]);

			if (iod > 0.0f) {
				C3D_FrameDrawOn(right);
				Mtx_PerspStereoTilt(&proj, C3D_AngleFromDegrees(cam.fovy), C3D_AspectRatioTop, 0.01f, 2.5f, iod, focus, false);
				render_camera(&proj, &view);
				draw_scene(&scene, fmt, loads[load_i]);
			}
		}

		char view_text[64];
		if (room_view)
			snprintf(view_text, sizeof(view_text), "view ROOM yaw %.0f pitch %.0f dist %.2f", room.yaw, room.pitch, room.dist);
		else
			snprintf(view_text, sizeof(view_text), "view TOP-DOWN height %.2f", top_h);
		double avg, worst;
		stats_summary(&st, &avg, &worst);
		if (!ok)
			snprintf(line, sizeof(line), "LOAD ERROR\n%s", err);
		else
			snprintf(line, sizeof(line),
			         "UR 3DS - M0 stereo test\n\n"
			         "frame avg %.2f ms (%.1f fps)\nframe worst %.2f ms\ncpu %.2f ms   gpu %.2f ms\n\n"
			         "texture %s   cull %s\nload x%d = %d tris, %d draws/eye\n3D slider %.2f\n"
			         "%s\n\n"
			         "%s%s",
			         avg, avg > 0 ? 1000.0 / avg : 0.0, worst, st.cpu_ms, st.gpu_ms,
			         fmt_name[fmt], cull ? "on" : "off", loads[load_i], scene.tris * loads[load_i],
			         scene.node_count * loads[load_i], slider,
			         view_text,
			         bench ? "BENCH RUNNING...\n" : "A tex  X load  B cull  Y bench\nD-pad up room / down top  L/R zoom\nPad orbit (room)  SELECT reset\n",
			         bench_log);

		C2D_TextBufClear(textbuf);
		C2D_Text text;
		C2D_TextParse(&text, textbuf, line);
		C2D_Prepare();
		C2D_TargetClear(bottom, C2D_Color32(0x1A, 0x22, 0x40, 0xFF));
		C2D_SceneBegin(bottom);
		C2D_DrawText(&text, C2D_WithColor, 8.0f, 6.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xF0, 0xE6, 0xD0, 0xFF));
		C3D_FrameEnd(0);
		stats_frame(&st);
	}

	if (ok)
		scene_free(&scene);
	for (int t = 0; t < SCENE_MAX_TEX; t++)
		for (int f = 0; f < FMT_COUNT; f++)
			if (tex[t][f].data)
				C3D_TexDelete(&tex[t][f]);
	render_exit();
	C2D_TextBufDelete(textbuf);
	C2D_Fini();
	C3D_Fini();
	romfsExit();
	gfxExit();
	return 0;
}
