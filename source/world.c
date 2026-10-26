#include "world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sfx.h"

/* Board geometry in glTF metres (asset pack board_layout.json). */
#define BOARD_TOP 0.024f
/* Column 0's centre per board: the Ur board and the 12-long Twenty Squares
 * board are both centred on the table. */
static const float cell_x0[UR_BOARD_COUNT] = { -0.1316875f, -0.2069375f };
#define CELL_X0 (cell_x0[board])
#define CELL_DX 0.037625f
#define ROW_DZ 0.0366667f
#define CELL_HALF 0.0188f
/* Pool (waiting) and borne-off rows on the table, in front of each player. */
#define ROW_Z 0.080f
#define SLOT_X0 -0.140f
#define SLOT_DX 0.0235f
#define MAX_PATH 17

typedef struct { float x, y, z; } V3;

typedef struct {
	int step, slot;          /* logical place; slot for pool/off rows */
	V3 pos;
	V3 path[MAX_PATH];       /* animation waypoints, path[0] = start */
	float dur[MAX_PATH], arc[MAX_PATH];
	int npath, seg;
	float t, delay;
	int land_sfx;            /* sound for the final landing */
	float lift;              /* animated selection lift */
	float yaw;
} Piece;

typedef struct {
	bool on;
	V3 start, land;
	float q[4];              /* final orientation (w x y z) */
	float spin_axis[3], spin;
	float t, delay;
	bool rattled;
} Die;

static Piece pc[2][UR_PIECES];
static int npieces = UR_PIECES;   /* in play on this board; the rest stay hidden */
static Die dice[4];
static int board;           /* UR_BOARD_UR or UR_BOARD_TWENTY */
static int mesh_board[UR_BOARD_COUNT] = { -1, -1 };
static u8 hl[UR_MAX_TRACK + 2];
static int hl_player;
static float clock_s;

static const Scene *sc;
static int mesh_light = -1, mesh_dark = -1, mesh_die = -1;
static float die_verts[64][3];
static int die_nverts;
static C3D_Tex decal_tex;
static Mesh decal[4];   /* blob, ring, frame, glow */

/* ---- small quaternion helpers (w, x, y, z), Hamilton convention ---- */

static void q_axis(float q[4], float ax, float ay, float az, float a)
{
	float l = sqrtf(ax * ax + ay * ay + az * az);
	float s = sinf(a / 2) / (l > 0 ? l : 1);
	q[0] = cosf(a / 2);
	q[1] = ax * s;
	q[2] = ay * s;
	q[3] = az * s;
}

static void q_mul(float o[4], const float a[4], const float b[4])
{
	float r[4] = {
		a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
		a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
		a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
		a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0],
	};
	memcpy(o, r, sizeof(r));
}

/* 3x3 rotation rows from a unit quaternion. */
static void q_rows(const float q[4], float m[3][3])
{
	float w = q[0], x = q[1], y = q[2], z = q[3];
	m[0][0] = 1 - 2 * (y * y + z * z); m[0][1] = 2 * (x * y - w * z); m[0][2] = 2 * (x * z + w * y);
	m[1][0] = 2 * (x * y + w * z); m[1][1] = 1 - 2 * (x * x + z * z); m[1][2] = 2 * (y * z - w * x);
	m[2][0] = 2 * (x * z - w * y); m[2][1] = 2 * (y * z + w * x); m[2][2] = 1 - 2 * (x * x + y * y);
}

static void mtx_rt(C3D_Mtx *out, float m[3][3], float tx, float ty, float tz)
{
	Mtx_Identity(out);
	for (int r = 0; r < 3; r++) {
		out->r[r].x = m[r][0];
		out->r[r].y = m[r][1];
		out->r[r].z = m[r][2];
	}
	out->r[0].w = tx;
	out->r[1].w = ty;
	out->r[2].w = tz;
}

static float frand(void) { return (float)rand() / (float)RAND_MAX; }
static float ease_out(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- geometry ---- */

static V3 cell_pos(int row, int col)
{
	V3 v = { CELL_X0 + col * CELL_DX, BOARD_TOP, (row - 1) * ROW_DZ };
	return v;
}

/* Waiting pieces fill from the entry side, borne-off pieces from the far
 * end; the two never meet because together they are at most a full set. On Ur
 * they sit in a row in front of the board; on Twenty Squares beside the arm. */
static V3 slot_pos(int player, bool off, int slot)
{
	float side = player == 0 ? 1.0f : -1.0f;
	if (board == UR_BOARD_TWENTY) {
		int col = off ? 11 - slot : 4 + slot;
		V3 v = { CELL_X0 + col * CELL_DX, 0, side * ROW_DZ };
		return v;
	}
	V3 v = { off ? -SLOT_X0 - slot * SLOT_DX : SLOT_X0 + slot * SLOT_DX, 0, side * ROW_Z };
	return v;
}

/* Height of the surface under (x, z): board top over a cell, table elsewhere. */
static float ground_at(float x, float z)
{
	int col = (int)floorf((x - CELL_X0) / CELL_DX + 0.5f);
	int row = (int)floorf(z / ROW_DZ + 0.5f) + 1;
	if (col < 0 || col >= UR_BOARDS[board].cols || row < 0 || row > 2)
		return 0;
	if (fabsf(x - (CELL_X0 + col * CELL_DX)) > CELL_HALF || fabsf(z - (row - 1) * ROW_DZ) > CELL_HALF)
		return 0;
	return ur_cell_exists(board, row, col) ? BOARD_TOP : 0;
}

static V3 step_pos(int player, int step, int slot)
{
	int row, col;
	if (step == 0)
		return slot_pos(player, false, slot);
	if (step == UR_BOARDS[board].track + 1)
		return slot_pos(player, true, slot);
	ur_step_cell(board, player, step, &row, &col);
	return cell_pos(row, col);
}

void world_step_pos(int player, int step, float *x, float *z)
{
	int n = 0;
	for (int i = 0; i < npieces; i++)
		if (pc[player][i].step == step)
			n++;
	V3 v = step_pos(player, step, step == 0 ? n - 1 : n);
	*x = v.x;
	*z = v.z;
}

/* ---- setup ---- */



bool world_init(const Scene *s)
{
	sc = s;
	for (int i = 0; i < s->node_count; i++) {
		const Node *n = &s->nodes[i];
		if (n->kind == KIND_LIGHT)
			mesh_light = n->mesh;
		else if (n->kind == KIND_DARK)
			mesh_dark = n->mesh;
		else if (n->kind == KIND_DIE)
			mesh_die = n->mesh;
		else if (n->kind == KIND_BOARD_UR)
			mesh_board[UR_BOARD_UR] = i;
		else if (n->kind == KIND_BOARD_TWENTY)
			mesh_board[UR_BOARD_TWENTY] = i;
	}
	if (mesh_light < 0 || mesh_dark < 0 || mesh_die < 0)
		return false;

	/* unique die vertices, for resting it flat on the table at any angle */
	const Mesh *dm = &s->meshes[mesh_die];
	const float *v = dm->vbo;
	die_nverts = 0;
	for (int i = 0; i < dm->vcount && die_nverts < 64; i++)
		memcpy(die_verts[die_nverts++], v + i * 8, 3 * sizeof(float));

	if (!tex_load(&decal_tex, "romfs:/tex/decals.t3x"))
		return false;
	/* atlas cells: tex3ds puts t=1 at the top of the image */
	mesh_quad(&decal[0], 0.0f, 1.0f, 0.5f, 0.5f);
	mesh_quad(&decal[1], 0.5f, 1.0f, 1.0f, 0.5f);
	mesh_quad(&decal[2], 0.0f, 0.5f, 0.5f, 0.0f);
	mesh_quad(&decal[3], 0.5f, 0.5f, 1.0f, 0.0f);

	for (int p = 0; p < 2; p++)
		for (int i = 0; i < UR_PIECES; i++)
			pc[p][i].yaw = (float)(i * 0.9 + p * 0.4);
	return true;
}

void world_exit(void)
{
	C3D_TexDelete(&decal_tex);

	for (int i = 0; i < 4; i++) {
		linearFree(decal[i].vbo);
		linearFree(decal[i].ibo);
	}
}

int world_board(void)
{
	return board;
}

void world_reset(const UrPos *p)
{
	board = p->board < UR_BOARD_COUNT ? p->board : UR_BOARD_UR;
	npieces = ur_pieces(board);
	int track = UR_BOARDS[board].track;
	for (int pl = 0; pl < 2; pl++) {
		int k = 0, pool = 0, off = 0;
		for (int i = 0; i < p->pool[pl]; i++, k++) {
			pc[pl][k].step = 0;
			pc[pl][k].slot = pool++;
		}
		for (int s = 1; s <= track; s++)
			if (p->mask[pl] & (1u << s)) {
				pc[pl][k].step = s;
				pc[pl][k].slot = 0;
				k++;
			}
		for (int i = 0; i < p->done[pl]; i++, k++) {
			pc[pl][k].step = track + 1;
			pc[pl][k].slot = off++;
		}
		for (int i = 0; i < npieces; i++) {
			Piece *q = &pc[pl][i];
			q->pos = step_pos(pl, q->step, q->slot);
			q->npath = 0;
			q->lift = 0;
		}
	}
	memset(hl, 0, sizeof(hl));
}

/* ---- animation ---- */

static void path_begin(Piece *q, float delay, int land_sfx)
{
	q->path[0] = q->pos;
	q->npath = 1;
	q->seg = 0;
	q->t = 0;
	q->delay = delay;
	q->land_sfx = land_sfx;
}

static void path_add(Piece *q, V3 to, float dur, float arc)
{
	if (q->npath >= MAX_PATH)
		return;
	q->dur[q->npath - 1] = dur;
	q->arc[q->npath - 1] = arc;
	q->path[q->npath++] = to;
}

static Piece *find_piece(int pl, int step)
{
	Piece *best = NULL;
	for (int i = 0; i < npieces; i++) {
		Piece *q = &pc[pl][i];
		if (q->step != step)
			continue;
		/* from the pool, take the piece nearest the entry (highest slot) */
		if (!best || (step == 0 && q->slot > best->slot))
			best = q;
	}
	return best;
}

static int count_step(int pl, int step)
{
	int n = 0;
	for (int i = 0; i < npieces; i++)
		n += pc[pl][i].step == step;
	return n;
}

void world_move(const UrMoveResult *r)
{
	int pl = r->player, opp = pl ^ 1;
	Piece *q = find_piece(pl, r->from);
	if (!q)
		return;
	path_begin(q, 0, r->captured ? SFX_THUNK : SFX_CLACK);
	int s = r->from;
	if (s == 0) {
		s = 1;
		path_add(q, step_pos(pl, 1, 0), 0.24f, 0.022f);
	}
	int off = UR_BOARDS[board].track + 1;
	for (; s < r->to && s < off - 1; s++)
		path_add(q, step_pos(pl, s + 1, 0), 0.14f, 0.010f);
	if (r->to == off)
		path_add(q, step_pos(pl, off, count_step(pl, off)), 0.30f, 0.024f);
	q->step = r->to;
	q->slot = r->to == off ? count_step(pl, off) - 1 : 0;

	if (r->captured) {
		/* the victim waits for the mover to land, then flies home */
		float wait = 0;
		for (int i = 0; i < q->npath - 1; i++)
			wait += q->dur[i];
		for (int i = 0; i < npieces; i++) {
			Piece *v = &pc[opp][i];
			if (v->step != r->to)
				continue;
			int slot = count_step(opp, 0);
			path_begin(v, wait + 0.05f, SFX_CLACK);
			path_add(v, step_pos(opp, 0, slot), 0.45f, 0.045f);
			v->step = 0;
			v->slot = slot;
			break;
		}
	}
	/* close the gap in the pool: everyone slides toward slot 0 */
	if (r->from == 0) {
		int order = 0;
		for (int slot = 0; slot < npieces; slot++)
			for (int i = 0; i < npieces; i++) {
				Piece *w = &pc[pl][i];
				if (w->step == 0 && w->slot == slot && w->npath == 0) {
					w->slot = order++;
				}
			}
		for (int i = 0; i < npieces; i++) {
			Piece *w = &pc[pl][i];
			V3 want = step_pos(pl, 0, w->slot);
			if (w->step == 0 && w->npath == 0 && (fabsf(want.x - w->pos.x) > 1e-4f)) {
				path_begin(w, 0.1f, -1);
				path_add(w, want, 0.2f, 0.0f);
			}
		}
	}
}

static bool piece_update(Piece *q, float dt)
{
	if (q->npath < 2)
		return false;
	if (q->delay > 0) {
		q->delay -= dt;
		return true;
	}
	q->t += dt;
	while (q->seg < q->npath - 1 && q->t >= q->dur[q->seg]) {
		q->t -= q->dur[q->seg];
		q->seg++;
		V3 p = q->path[q->seg];
		float pan = clampf(p.x / 0.15f, -1, 1);
		if (q->seg == q->npath - 1) {
			if (q->land_sfx >= 0)
				sfx_play(q->land_sfx, pan, 1.0f);
		} else {
			sfx_play(SFX_TAP, pan, 0.45f);
		}
	}
	if (q->seg >= q->npath - 1) {
		q->pos = q->path[q->npath - 1];
		q->npath = 0;
		return false;
	}
	float u = q->t / q->dur[q->seg];
	V3 a = q->path[q->seg], b = q->path[q->seg + 1];
	float e = u * u * (3 - 2 * u);
	q->pos.x = a.x + (b.x - a.x) * e;
	q->pos.z = a.z + (b.z - a.z) * e;
	q->pos.y = a.y + (b.y - a.y) * u + q->arc[q->seg] * 4 * u * (1 - u);
	return true;
}

/* Directions from the die's centre to its four tips (asset pack dice_scoring.json). */
static const float tip_dir[4][3] = {
	{ -0.8221254f, -0.3143421f, 0.4746565f },
	{ 0.8221254f, -0.3143421f, 0.4746565f },
	{ 0.0f, -0.3143411f, -0.9493101f },
	{ 0.0f, 1.0f, 0.0f },
};
static const bool tip_marked[4] = { true, false, false, true };

void world_roll(int player, const bool marked[4])
{
	/* Ur: in the board's notch on the roller's side; Twenty: on the table
	 * in front of the roller, under the block. */
	static const float lx[UR_BOARD_COUNT][2] = { { 0.0195f, 0.0555f }, { -0.170f, -0.135f } };
	static const float lz[UR_BOARD_COUNT][2] = { { 0.030f, 0.052f }, { 0.080f, 0.102f } };
	float side = player == 0 ? 1.0f : -1.0f;
	for (int i = 0; i < 4; i++) {
		Die *d = &dice[i];
		/* choose a tip with the wanted marking and turn it straight up */
		int cand[2], n = 0;
		for (int k = 0; k < 4; k++)
			if (tip_marked[k] == marked[i])
				cand[n++] = k;
		const float *v = tip_dir[cand[rand() % n]];
		float align[4], yaw[4];
		float dot = v[1];
		if (dot > 0.9999f) {
			align[0] = 1; align[1] = align[2] = align[3] = 0;
		} else {
			/* axis = v x up = (-vz, 0, vx) */
			q_axis(align, -v[2], 0, v[0], acosf(clampf(dot, -1, 1)));
		}
		q_axis(yaw, 0, 1, 0, frand() * 6.2832f);
		q_mul(d->q, yaw, align);

		d->land.x = lx[board][i & 1] + (frand() - 0.5f) * 0.004f;
		d->land.z = side * (lz[board][i >> 1] + (frand() - 0.5f) * 0.003f);
		d->land.y = 0;
		d->start.x = d->land.x + (frand() - 0.5f) * 0.03f;
		d->start.z = side * 0.115f;
		d->start.y = 0.075f + frand() * 0.02f;
		d->spin_axis[0] = frand() - 0.5f;
		d->spin_axis[1] = frand() - 0.5f;
		d->spin_axis[2] = frand() - 0.5f;
		d->spin = 9.0f + frand() * 5.0f;
		d->t = 0;
		d->delay = i * 0.035f;
		d->rattled = false;
		d->on = true;
	}
}

void world_clear_dice(void)
{
	for (int i = 0; i < 4; i++)
		dice[i].on = false;
}

#define DIE_FALL 0.30f
#define DIE_B1 0.15f
#define DIE_B2 0.09f
#define DIE_TOTAL (DIE_FALL + DIE_B1 + DIE_B2)

static bool die_update(Die *d, float dt)
{
	if (!d->on)
		return false;
	if (d->delay > 0) {
		d->delay -= dt;
		return true;
	}
	if (d->t >= DIE_TOTAL)
		return false;
	d->t += dt;
	if (!d->rattled && d->t >= DIE_FALL) {
		d->rattled = true;
		if (d == &dice[0])
			sfx_play(SFX_RATTLE, d->land.x / 0.15f, 1.0f);
	}
	return d->t < DIE_TOTAL;
}

/* Rotation, then lift so the lowest tip touches y = 0. */
static void die_matrix(const Die *d, C3D_Mtx *out)
{
	float q[4], spin[4], m[3][3];
	float t = clampf(d->t / DIE_TOTAL, 0, 1);
	q_axis(spin, d->spin_axis[0], d->spin_axis[1], d->spin_axis[2], d->spin * (1 - ease_out(t)));
	q_mul(q, d->q, spin);
	q_rows(q, m);

	float miny = 1e9f;
	for (int i = 0; i < die_nverts; i++) {
		const float *v = die_verts[i];
		float y = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2];
		if (y < miny)
			miny = y;
	}
	float x, y, z, tt = d->t;
	if (tt < DIE_FALL) {
		float u = tt / DIE_FALL;
		x = d->start.x + (d->land.x - d->start.x) * ease_out(u);
		z = d->start.z + (d->land.z - d->start.z) * ease_out(u);
		y = d->start.y * (1 - u * u);
	} else {
		x = d->land.x;
		z = d->land.z;
		if (tt < DIE_FALL + DIE_B1) {
			float u = (tt - DIE_FALL) / DIE_B1;
			y = 0.010f * 4 * u * (1 - u);
		} else if (tt < DIE_TOTAL) {
			float u = (tt - DIE_FALL - DIE_B1) / DIE_B2;
			y = 0.003f * 4 * u * (1 - u);
		} else {
			y = 0;
		}
	}
	mtx_rt(out, m, x, y - miny, z);
}

bool world_busy(void)
{
	for (int p = 0; p < 2; p++)
		for (int i = 0; i < npieces; i++)
			if (pc[p][i].npath >= 2)
				return true;
	for (int i = 0; i < 4; i++)
		if (dice[i].on && (dice[i].delay > 0 || dice[i].t < DIE_TOTAL))
			return true;
	return false;
}

void world_update(float dt)
{
	clock_s += dt;
	for (int p = 0; p < 2; p++)
		for (int i = 0; i < npieces; i++) {
			Piece *q = &pc[p][i];
			piece_update(q, dt);
			float want = (p == hl_player && (hl[q->step] & HL_SELECTED) && q->npath == 0 &&
			              q == find_piece(p, q->step)) ? 0.005f : 0.0f;
			q->lift += (want - q->lift) * clampf(dt * 14, 0, 1);
		}
	for (int i = 0; i < 4; i++)
		die_update(&dice[i], dt);
}

void world_highlight(int player, const u8 *flags)
{
	hl_player = player;
	memcpy(hl, flags, sizeof(hl));
}

/* ---- drawing ---- */

static void draw_decal(int kind, float x, float y, float z, float size, const float tint[4])
{
	C3D_Mtx m;
	Mtx_Identity(&m);
	Mtx_Translate(&m, x, y, z, true);
	Mtx_Scale(&m, size, 1, size);
	render_mesh(&decal[kind], &m, &decal_tex, tint);
}

void world_draw(int fmt, C3D_Tex tex[][2], bool flames_on)
{
	static const float white[4] = { 1, 1, 1, 1 };
	C3D_Mtx m;

	for (int i = 0; i < sc->node_count; i++) {
		const Node *n = &sc->nodes[i];
		bool this_board = (n->kind == KIND_BOARD_UR && board == UR_BOARD_UR) ||
		                  (n->kind == KIND_BOARD_TWENTY && board == UR_BOARD_TWENTY);
		if (n->kind == KIND_STATIC || this_board || (n->kind == KIND_FLAME && flames_on)) {
			const Mesh *me = &sc->meshes[n->mesh];
			float f = 0.85f + 0.15f * sinf(clock_s * 11 + i) * sinf(clock_s * 7.3f + i * 2);
			const float flick[4] = { 1, f, f * 0.9f, 1 };
			render_mesh(me, &n->model, &tex[me->material][fmt], n->kind == KIND_FLAME ? flick : white);
		}
	}
	for (int p = 0; p < 2; p++) {
		const Mesh *me = &sc->meshes[p == 0 ? mesh_light : mesh_dark];
		for (int i = 0; i < npieces; i++) {
			Piece *q = &pc[p][i];
			Mtx_Identity(&m);
			Mtx_Translate(&m, q->pos.x, q->pos.y + q->lift, q->pos.z, true);
			Mtx_RotateY(&m, q->yaw, true);
			render_mesh(me, &m, &tex[me->material][fmt], white);
		}
	}
	const Mesh *dm = &sc->meshes[mesh_die];
	for (int i = 0; i < 4; i++)
		if (dice[i].on) {
			die_matrix(&dice[i], &m);
			render_mesh(dm, &m, &tex[dm->material][fmt], white);
		}

	/* decals: alpha blended, no depth writes */
	render_decal_mode(true);
	for (int p = 0; p < 2; p++)
		for (int i = 0; i < npieces; i++) {
			Piece *q = &pc[p][i];
			float g = ground_at(q->pos.x, q->pos.z);
			float h = q->pos.y + q->lift - g;
			float a = clampf(0.55f - h * 12, 0.15f, 0.55f);
			const float sh[4] = { 0.08f, 0.05f, 0.03f, a };
			draw_decal(0, q->pos.x + 0.0012f, g + 0.0003f, q->pos.z + 0.0018f, 0.030f + h * 0.4f, sh);
		}
	for (int i = 0; i < 4; i++)
		if (dice[i].on) {
			const Die *d = &dice[i];
			const float sh[4] = { 0.08f, 0.05f, 0.03f, 0.35f };
			float x = d->t < DIE_FALL ? d->start.x + (d->land.x - d->start.x) * ease_out(d->t / DIE_FALL) : d->land.x;
			float z = d->t < DIE_FALL ? d->start.z + (d->land.z - d->start.z) * ease_out(d->t / DIE_FALL) : d->land.z;
			draw_decal(0, x + 0.001f, 0.0003f, z + 0.0015f, 0.026f, sh);
		}

	float pulse = 0.65f + 0.35f * sinf(clock_s * 5.5f);
	int off = UR_BOARDS[board].track + 1;
	for (int s = 0; s <= off; s++) {
		if (!hl[s])
			continue;
		bool sel = hl[s] & HL_SELECTED;
		if (hl[s] & HL_MOVABLE) {
			Piece *q = find_piece(hl_player, s);
			if (q && q->npath == 0) {
				const float gold[4] = { 0.85f, 0.65f, 0.24f, sel ? 1.0f : 0.75f * pulse };
				draw_decal(1, q->pos.x, q->pos.y + 0.0004f, q->pos.z, sel ? 0.032f : 0.029f, gold);
			}
		}
		if (hl[s] & HL_TARGET) {
			V3 v = step_pos(hl_player, s, s == off ? count_step(hl_player, off) : 0);
			const float red[4] = { 0.77f, 0.33f, 0.18f, sel ? 1.0f : 0.8f * pulse };
			draw_decal(2, v.x, v.y + 0.0004f, v.z, s == off ? 0.026f : 0.034f, red);
		}
	}
	render_decal_mode(false);
}
