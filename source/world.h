/* The 3D table: where every counter and die is, their animations, the
 * highlight decals, and drawing it all for one eye. */
#ifndef UR_WORLD_H
#define UR_WORLD_H

#include <stdbool.h>
#include "scene.h"
#include "ur_rules.h"

/* Highlight flags per step, set by the game flow each frame. */
enum {
	HL_MOVABLE = 1,   /* this player's piece on this step can move */
	HL_TARGET = 2,    /* a legal destination */
	HL_SELECTED = 4,  /* the move picked with the D-pad / touch */
};

bool world_init(const Scene *s);
void world_exit(void);

/* Snap every piece to the position (no animation). */
void world_reset(const UrPos *p);

/* Animate a move already applied to the rules. Captured piece (if any) is
 * sent home after the mover lands. */
void world_move(const UrMoveResult *r);

/* Throw the dice for `player`; marked[i] says which dice land marked-tip up. */
void world_roll(int player, const bool marked[4]);
void world_clear_dice(void);

bool world_busy(void);              /* pieces or dice still moving */
void world_update(float dt);

/* Highlights for `player`: flags per step 0..15 (0 = the pool, 15 = off). */
void world_highlight(int player, const u8 flags[16]);

/* World position of a piece standing on `step` (for pool/off: the next slot). */
void world_step_pos(int player, int step, float *x, float *z);

void world_draw(int tex_fmt, C3D_Tex tex[][2], bool flames_on);

#endif
