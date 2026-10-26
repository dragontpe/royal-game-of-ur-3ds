#ifndef UR_SCENE_H
#define UR_SCENE_H

#include <3ds.h>
#include <citro3d.h>
#include <stdbool.h>

enum { KIND_STATIC, KIND_LIGHT, KIND_DARK, KIND_DIE };

typedef struct {
	int material;       /* 0 board, 1 pieces atlas, 2+ extra textures */
	int vcount, icount;
	void *vbo;          /* linear memory: pos3 uv2 nrm3 floats */
	u16 *ibo;           /* linear memory */
} Mesh;

typedef struct {
	int mesh, kind;
	C3D_Mtx model;
} Node;

typedef struct {
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

#endif
