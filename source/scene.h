#ifndef UR_SCENE_H
#define UR_SCENE_H

#include <3ds.h>
#include <citro3d.h>
#include <stdbool.h>

enum { KIND_STATIC, KIND_LIGHT, KIND_DARK, KIND_DIE, KIND_FLAME, KIND_BOARD_UR, KIND_BOARD_TWENTY };

#define SCENE_MAX_TEX 8

typedef struct {
	int material;       /* index into Scene.tex_names */
	int vcount, icount;
	void *vbo;          /* linear memory: pos3 uv2 nrm3 floats */
	u16 *ibo;           /* linear memory */
} Mesh;

typedef struct {
	int mesh, kind;
	C3D_Mtx model;
} Node;

typedef struct {
	int tex_count;
	char tex_names[SCENE_MAX_TEX][32];
	int mesh_count, node_count;
	Mesh *meshes;
	Node *nodes;
	int tris;           /* triangles for one pass over all nodes */
} Scene;

bool scene_load(Scene *s, const char *path);
void scene_free(Scene *s);

bool tex_load(C3D_Tex *tex, const char *path);

bool render_init(void);
void render_exit(void);
/* Re-apply all 3D GPU state; call before 3D drawing each frame (citro2d changes it). */
void render_bind(void);
void render_camera(const C3D_Mtx *proj, const C3D_Mtx *view);
void render_mesh(const Mesh *m, const C3D_Mtx *model, C3D_Tex *tex, const float tint[4]);
/* Alpha-blended decals: no depth writes, no culling. Call with false to restore. */
void render_decal_mode(bool on);
/* Unit quad in the XZ plane (normal +Y) with the given texture corners. */
bool mesh_quad(Mesh *m, float u0, float v0, float u1, float v1);

#endif
