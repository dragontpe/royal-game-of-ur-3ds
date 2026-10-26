#include "scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tex3ds.h>

#include "scene_shbin.h"

static DVLB_s *dvlb;
static shaderProgram_s program;
static int u_proj, u_modelview, u_tint;
static C3D_Mtx cur_proj, cur_view;

static bool read_all(const char *path, u8 **data, size_t *size)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*data = malloc(*size);
	bool ok = *data && fread(*data, 1, *size, f) == *size;
	fclose(f);
	return ok;
}

/* glTF matrices are column major; C3D_Mtx rows are stored w,z,y,x. */
static void mtx_from_gltf(C3D_Mtx *out, const float *g)
{
	for (int r = 0; r < 4; r++) {
		out->r[r].x = g[0 * 4 + r];
		out->r[r].y = g[1 * 4 + r];
		out->r[r].z = g[2 * 4 + r];
		out->r[r].w = g[3 * 4 + r];
	}
}

bool scene_load(Scene *s, const char *path)
{
	u8 *data;
	size_t size;
	memset(s, 0, sizeof(*s));
	if (!read_all(path, &data, &size))
		return false;

	const u8 *p = data;
	if (size < 16 || memcmp(p, "URS2", 4) != 0)
		goto fail;
	u32 tc, mc, nc;
	memcpy(&tc, p + 4, 4);
	memcpy(&mc, p + 8, 4);
	memcpy(&nc, p + 12, 4);
	p += 16;
	if (tc > SCENE_MAX_TEX)
		goto fail;
	s->tex_count = tc;
	for (u32 i = 0; i < tc; i++, p += 32) {
		memcpy(s->tex_names[i], p, 31);
		s->tex_names[i][31] = 0;
	}
	s->meshes = calloc(mc, sizeof(Mesh));
	s->nodes = calloc(nc, sizeof(Node));
	s->mesh_count = mc;
	s->node_count = nc;

	for (u32 i = 0; i < mc; i++) {
		u32 hdr[3];
		memcpy(hdr, p, 12);
		p += 12;
		Mesh *m = &s->meshes[i];
		m->material = hdr[0];
		m->vcount = hdr[1];
		m->icount = hdr[2];
		size_t vbytes = m->vcount * 8 * sizeof(float);
		size_t ibytes = m->icount * sizeof(u16);
		m->vbo = linearAlloc(vbytes);
		m->ibo = linearAlloc(ibytes);
		memcpy(m->vbo, p, vbytes);
		p += vbytes;
		memcpy(m->ibo, p, ibytes);
		p += (ibytes + 3) & ~3u;
	}
	for (u32 i = 0; i < nc; i++) {
		u32 hdr[2];
		float g[16];
		memcpy(hdr, p, 8);
		memcpy(g, p + 8, 64);
		p += 72;
		s->nodes[i].mesh = hdr[0];
		s->nodes[i].kind = hdr[1];
		mtx_from_gltf(&s->nodes[i].model, g);
		s->tris += s->meshes[hdr[0]].icount / 3;
	}
	free(data);
	return true;
fail:
	free(data);
	return false;
}

void scene_free(Scene *s)
{
	for (int i = 0; i < s->mesh_count; i++) {
		linearFree(s->meshes[i].vbo);
		linearFree(s->meshes[i].ibo);
	}
	free(s->meshes);
	free(s->nodes);
	memset(s, 0, sizeof(*s));
}

bool tex_load(C3D_Tex *tex, const char *path)
{
	/* VRAM first; fall back to linear memory if VRAM is full. */
	Tex3DS_Texture t3x = NULL;
	for (int vram = 1; vram >= 0 && !t3x; vram--) {
		FILE *f = fopen(path, "rb");
		if (!f)
			return false;
		t3x = Tex3DS_TextureImportStdio(f, tex, NULL, vram);
		fclose(f);
	}
	if (!t3x)
		return false;
	Tex3DS_TextureFree(t3x);
	C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	return true;
}

bool render_init(void)
{
	dvlb = DVLB_ParseFile((u32 *)scene_shbin, scene_shbin_size);
	if (!dvlb)
		return false;
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
	u_proj = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	u_modelview = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	u_tint = shaderInstanceGetUniformLocation(program.vertexShader, "tint");
	return true;
}

void render_exit(void)
{
	shaderProgramFree(&program);
	DVLB_Free(dvlb);
}

void render_bind(void)
{
	C3D_BindProgram(&program);

	C3D_AttrInfo *attr = C3D_GetAttrInfo();
	AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3); /* position */
	AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2); /* texcoord */
	AttrInfo_AddLoader(attr, 2, GPU_FLOAT, 3); /* normal (unused by the shader for now) */

	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	C3D_CullFace(GPU_CULL_BACK_CCW);

	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	for (int i = 1; i < 6; i++)
		C3D_TexEnvInit(C3D_GetTexEnv(i));
}

void render_camera(const C3D_Mtx *proj, const C3D_Mtx *view)
{
	cur_proj = *proj;
	cur_view = *view;
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_proj, &cur_proj);
}

void render_mesh(const Mesh *m, const C3D_Mtx *model, C3D_Tex *tex, const float tint[4])
{
	C3D_Mtx mv;
	Mtx_Multiply(&mv, &cur_view, model);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_modelview, &mv);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, u_tint, tint[0], tint[1], tint[2], tint[3]);

	C3D_BufInfo *buf = C3D_GetBufInfo();
	BufInfo_Init(buf);
	BufInfo_Add(buf, m->vbo, 8 * sizeof(float), 3, 0x210);

	C3D_TexBind(0, tex);
	C3D_DrawElements(GPU_TRIANGLES, m->icount, C3D_UNSIGNED_SHORT, m->ibo);
}

void render_decal_mode(bool on)
{
	C3D_DepthTest(true, GPU_GREATER, on ? GPU_WRITE_COLOR : GPU_WRITE_ALL);
	C3D_CullFace(on ? GPU_CULL_NONE : GPU_CULL_BACK_CCW);
}

bool mesh_quad(Mesh *m, float u0, float v0, float u1, float v1)
{
	const float v[4][8] = {
		{ -0.5f, 0, -0.5f, u0, v0, 0, 1, 0 },
		{ -0.5f, 0, 0.5f, u0, v1, 0, 1, 0 },
		{ 0.5f, 0, 0.5f, u1, v1, 0, 1, 0 },
		{ 0.5f, 0, -0.5f, u1, v0, 0, 1, 0 },
	};
	static const u16 idx[6] = { 0, 1, 2, 0, 2, 3 };
	m->material = 0;
	m->vcount = 4;
	m->icount = 6;
	m->vbo = linearAlloc(sizeof(v));
	m->ibo = linearAlloc(sizeof(idx));
	if (!m->vbo || !m->ibo)
		return false;
	memcpy(m->vbo, v, sizeof(v));
	memcpy(m->ibo, idx, sizeof(idx));
	return true;
}
