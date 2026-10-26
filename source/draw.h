/* 2D drawing layer, ported from Gammon-X: a sprite atlas
 * (romfs:/gfx/art.t3x, indices in art.h), flat rounded panels and our own
 * pixel-exact text renderer. Nothing is scaled at runtime. */
#ifndef UR_DRAW_H
#define UR_DRAW_H

#include <citro2d.h>
#include "art.h"
#include "glyphs.h"
#include "theme.h"

typedef enum { A_LEFT, A_CENTER, A_RIGHT } align_t;
typedef enum { P_TILE3, P_TILE6, P_HI6, P_BTN3, P_BTNLO3, P_BOX3, P_SCRIM6 } panel_kind;

int draw_init(void);        /* 0 if art and fonts loaded */
void draw_exit(void);

void spr(size_t idx, float x, float y);
/* Sprite rotated about its centre (cx, cy), angle in radians. */
void spr_rot(size_t idx, float cx, float cy, float angle);
/* A tappable sprite: sinks 1 px for a moment after the stylus lands on it. */
void spr_btn(size_t idx, float x, float y);
void draw_touch(int x, int y);
int spr_w(size_t idx);
int spr_h(size_t idx);

void panel(panel_kind k, float x, float y, float w, float h);
/* A panel that sinks 1 px when just touched. */
void panel_btn(panel_kind k, float x, float y, float w, float h);
void draw_rect(float x, float y, float w, float h, u32 c);
void draw_fade(float amount, int w, int h, u32 c);

/* Text vertically centred on y (middle of a capital). Colour is baked in the style. */
void text(text_style ts, float x, float y, align_t a, const char *fmt, ...)
	__attribute__((format(printf, 5, 6)));
float text_w(text_style ts, const char *s);
/* Word-wrapped paragraph; returns the y after the last line. */
float text_wrap(text_style ts, float x, float y, float w, float line_h, const char *s);

int in_rect(int px, int py, float x, float y, float w, float h);
/* true for ~140 ms after a touch inside the rect (button press feedback) */
int touched_recently(float x, float y, float w, float h);

#endif
