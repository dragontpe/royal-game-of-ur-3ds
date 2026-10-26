/* M0 stereo test cart: Frank's asset pack in stereo on the top screen,
 * with a perf readout on the bottom screen.
 *
 * Circle pad orbit, L/R zoom, A texture format, X load multiplier,
 * B back-face culling, Y timed bench (writes sdmc:/ur-3ds-bench.txt),
 * SELECT reset camera, START quit. */
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
static const char *tex_file[2][FMT_COUNT] = {
	{ "romfs:/tex/ur_board_rgb565.t3x", "romfs:/tex/ur_board_etc1.t3x" },
	{ "romfs:/tex/ur_pieces_rgb565.t3x", "romfs:/tex/ur_pieces_etc1.t3x" },
};
static C3D_Tex tex[2][FMT_COUNT];

static const int loads[] = { 1, 2, 4, 8, 16 };
#define LOAD_COUNT (int)(sizeof(loads) / sizeof(loads[0]))

typedef struct {
	float yaw, pitch, dist, fovy;
} Camera;

static const Camera cam_default = { 25.0f, 50.0f, 0.33f, 40.0f };

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

static void view_matrix(C3D_Mtx *view, const Camera *c, C3D_FVec target)
{
	float yaw = C3D_AngleFromDegrees(c->yaw), pitch = C3D_AngleFromDegrees(c->pitch);
	C3D_FVec eye = FVec3_New(target.x + c->dist * cosf(pitch) * sinf(yaw),
	                         target.y + c->dist * sinf(pitch),
	                         target.z + c->dist * cosf(pitch) * cosf(yaw));
	Mtx_LookAt(view, eye, target, FVec3_New(0, 1, 0), false);
}

static void draw_scene(const Scene *s, int fmt, int load)
{
	static const float white[4] = { 1, 1, 1, 1 };
	for (int k = 0; k < load; k++)
		for (int i = 0; i < s->node_count; i++) {
			const Node *n = &s->nodes[i];
			const Mesh *m = &s->meshes[n->mesh];
			render_mesh(m, &n->model, &tex[m->material > 0 ? 1 : 0][fmt], white);
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
	for (int t = 0; ok && t < 2; t++)
		for (int f = 0; f < FMT_COUNT; f++)
			if (!tex_load(&tex[t][f], tex_file[t][f])) {
				ok = false;
				snprintf(err, sizeof(err), "%s failed", tex_file[t][f]);
			}

	Camera cam = cam_default;
	C3D_FVec target = FVec3_New(0.0f, 0.012f, 0.0f);
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
		if (down & KEY_SELECT)
			cam = cam_default;
		if ((down & KEY_Y) && !bench) {
			bench = true;
			bench_i = 0;
			bench_log[0] = 0;
			bench_start = svcGetSystemTick();
		}

		circlePosition cp;
		hidCircleRead(&cp);
		if (abs(cp.dx) > 15)
			cam.yaw += cp.dx / 156.0f * 1.5f;
		if (abs(cp.dy) > 15)
			cam.pitch -= cp.dy / 156.0f * 1.0f;
		if (held & KEY_L)
			cam.dist *= 1.01f;
		if (held & KEY_R)
			cam.dist /= 1.01f;
		cam.pitch = cam.pitch < 10 ? 10 : cam.pitch > 89 ? 89 : cam.pitch;
		cam.dist = cam.dist < 0.15f ? 0.15f : cam.dist > 1.2f ? 1.2f : cam.dist;

		if (bench) {
			cam = cam_default;
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
		float iod = slider * cam.dist / 6.0f;
		C3D_Mtx view, proj;
		view_matrix(&view, &cam, target);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(left, C3D_CLEAR_ALL, CLEAR_TOP, 0);
		C3D_RenderTargetClear(right, C3D_CLEAR_ALL, CLEAR_TOP, 0);
		if (ok) {
			render_bind();
			C3D_CullFace(cull ? GPU_CULL_BACK_CCW : GPU_CULL_NONE);

			C3D_FrameDrawOn(left);
			Mtx_PerspStereoTilt(&proj, C3D_AngleFromDegrees(cam.fovy), C3D_AspectRatioTop, 0.01f, 5.0f, -iod, cam.dist, false);
			render_camera(&proj, &view);
			draw_scene(&scene, fmt, loads[load_i]);

			if (iod > 0.0f) {
				C3D_FrameDrawOn(right);
				Mtx_PerspStereoTilt(&proj, C3D_AngleFromDegrees(cam.fovy), C3D_AspectRatioTop, 0.01f, 5.0f, iod, cam.dist, false);
				render_camera(&proj, &view);
				draw_scene(&scene, fmt, loads[load_i]);
			}
		}

		double avg, worst;
		stats_summary(&st, &avg, &worst);
		if (!ok)
			snprintf(line, sizeof(line), "LOAD ERROR\n%s", err);
		else
			snprintf(line, sizeof(line),
			         "UR 3DS - M0 stereo test\n\n"
			         "frame avg %.2f ms (%.1f fps)\nframe worst %.2f ms\ncpu %.2f ms   gpu %.2f ms\n\n"
			         "texture %s   cull %s\nload x%d = %d tris/eye\n3D slider %.2f\n"
			         "cam yaw %.0f pitch %.0f dist %.2f\n\n"
			         "%s%s",
			         avg, avg > 0 ? 1000.0 / avg : 0.0, worst, st.cpu_ms, st.gpu_ms,
			         fmt_name[fmt], cull ? "on" : "off", loads[load_i], scene.tris * loads[load_i], slider,
			         cam.yaw, cam.pitch, cam.dist,
			         bench ? "BENCH RUNNING...\n" : "A tex  X load  B cull  Y bench\nPad orbit  L/R zoom  SELECT reset\n",
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
	for (int t = 0; t < 2; t++)
		for (int f = 0; f < FMT_COUNT; f++)
			C3D_TexDelete(&tex[t][f]);
	render_exit();
	C2D_TextBufDelete(textbuf);
	C2D_Fini();
	C3D_Fini();
	romfsExit();
	gfxExit();
	return 0;
}
