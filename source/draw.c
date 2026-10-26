/* 2D drawing layer. See draw.h. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "draw.h"

static C2D_SpriteSheet sheet;
static C2D_SpriteSheet font_sheet[TS_COUNT];
/* per style, per byte: glyph index + 1 (0 = missing) */
static unsigned char glyph_of[TS_COUNT][256];
static int touch_x = -100, touch_y = -100;
static u64 touch_at;

int draw_init(void)
{
	sheet = C2D_SpriteSheetLoad("romfs:/gfx/art.t3x");
	int ok = sheet != NULL;
	for (int i = 0; i < TS_COUNT; i++) {
		font_sheet[i] = C2D_SpriteSheetLoad(text_fonts[i].path);
		ok &= font_sheet[i] != NULL;
		for (int k = 0; k < text_fonts[i].nglyphs; k++)
			glyph_of[i][text_fonts[i].glyphs[k].ch] = (unsigned char)(k + 1);
	}
	return ok ? 0 : -1;
}

void draw_exit(void)
{
	for (int i = 0; i < TS_COUNT; i++)
		if (font_sheet[i])
			C2D_SpriteSheetFree(font_sheet[i]);
	if (sheet)
		C2D_SpriteSheetFree(sheet);
}

void spr(size_t idx, float x, float y)
{
	if (sheet)
		C2D_DrawImageAt(C2D_SpriteSheetGetImage(sheet, idx), (float)(int)(x + 0.5f), (float)(int)(y + 0.5f), 0.5f,
		                NULL, 1.0f, 1.0f);
}

void spr_rot(size_t idx, float cx, float cy, float angle)
{
	if (sheet)
		C2D_DrawImageAtRotated(C2D_SpriteSheetGetImage(sheet, idx), cx, cy, 0.5f, angle, NULL, 1.0f, 1.0f);
}

int spr_w(size_t idx)
{
	return sheet ? C2D_SpriteSheetGetImage(sheet, idx).subtex->width : 0;
}

int spr_h(size_t idx)
{
	return sheet ? C2D_SpriteSheetGetImage(sheet, idx).subtex->height : 0;
}

void draw_touch(int x, int y)
{
	touch_x = x;
	touch_y = y;
	touch_at = svcGetSystemTick();
}

int touched_recently(float x, float y, float w, float h)
{
	u64 ms = (svcGetSystemTick() - touch_at) / (SYSCLOCK_ARM11 / 1000);
	return ms < 140 && in_rect(touch_x, touch_y, x, y, w, h);
}

void spr_btn(size_t idx, float x, float y)
{
	float dy = touched_recently(x, y, spr_w(idx), spr_h(idx)) ? 1 : 0;
	spr(idx, x, y + dy);
}

void draw_fade(float amount, int w, int h, u32 c)
{
	if (amount <= 0)
		return;
	if (amount > 1)
		amount = 1;
	C2D_DrawRectSolid(0, 0, 0.5f, (float)w, (float)h, (c & 0x00FFFFFFu) | ((u32)(amount * 255) << 24));
}

void panel(panel_kind k, float x, float y, float w, float h)
{
	static const size_t corner[][4] = {
		{ art_c_tile3_tl_idx, art_c_tile3_tr_idx, art_c_tile3_bl_idx, art_c_tile3_br_idx },
		{ art_c_tile6_tl_idx, art_c_tile6_tr_idx, art_c_tile6_bl_idx, art_c_tile6_br_idx },
		{ art_c_hi6_tl_idx, art_c_hi6_tr_idx, art_c_hi6_bl_idx, art_c_hi6_br_idx },
		{ art_c_btn3_tl_idx, art_c_btn3_tr_idx, art_c_btn3_bl_idx, art_c_btn3_br_idx },
		{ art_c_btnlo3_tl_idx, art_c_btnlo3_tr_idx, art_c_btnlo3_bl_idx, art_c_btnlo3_br_idx },
		{ art_c_box3_tl_idx, art_c_box3_tr_idx, art_c_box3_bl_idx, art_c_box3_br_idx },
		{ art_c_scrim6_tl_idx, art_c_scrim6_tr_idx, art_c_scrim6_bl_idx, art_c_scrim6_br_idx },
	};
	static const int radius[] = { 3, 6, 6, 3, 3, 3, 6 };
	u32 col;
	switch (k) {
	case P_TILE3: case P_TILE6: col = COL_TILE; break;
	case P_HI6: col = COL_GROUND_HI; break;
	case P_BTN3: col = COL_BTN; break;
	case P_BTNLO3: col = COL_BTN_LO; break;
	case P_BOX3: col = COL_BOX; break;
	default: col = COL_SCRIM; break;
	}
	int r = radius[k];
	x = (float)(int)(x + 0.5f);
	y = (float)(int)(y + 0.5f);
	C2D_DrawRectSolid(x + r, y, 0.5f, w - 2 * r, h, col);
	C2D_DrawRectSolid(x, y + r, 0.5f, r, h - 2 * r, col);
	C2D_DrawRectSolid(x + w - r, y + r, 0.5f, r, h - 2 * r, col);
	spr(corner[k][0], x, y);
	spr(corner[k][1], x + w - r, y);
	spr(corner[k][2], x, y + h - r);
	spr(corner[k][3], x + w - r, y + h - r);
}

void panel_btn(panel_kind k, float x, float y, float w, float h)
{
	panel(k, x, y + (touched_recently(x, y, w, h) ? 1 : 0), w, h);
}

void draw_rect(float x, float y, float w, float h, u32 c)
{
	C2D_DrawRectSolid(x, y, 0.5f, w, h, c);
}

/* Next byte of a UTF-8 string as one of our 8-bit glyph codes. */
static unsigned char next_char(const char **p)
{
	const unsigned char *s = (const unsigned char *)*p;
	if (s[0] == 0xC2 && s[1]) {
		*p += 2;
		return s[1];
	}
	*p += 1;
	return s[0];
}

static float kern(const font_t *f, unsigned char a, unsigned char b)
{
	for (int i = 0; i < f->nkern; i++)
		if (f->kern[i].a == a && f->kern[i].b == b)
			return f->kern[i].k;
	return 0;
}

static float span_w(text_style ts, const char *s, const char *end)
{
	const font_t *f = &text_fonts[ts];
	float w = 0;
	unsigned char prev = 0;
	while (*s && (!end || s < end)) {
		unsigned char c = next_char(&s);
		int g = glyph_of[ts][c];
		if (!g)
			continue;
		if (prev)
			w += kern(f, prev, c);
		w += f->glyphs[g - 1].adv;
		prev = c;
	}
	return w;
}

float text_w(text_style ts, const char *s)
{
	return span_w(ts, s, NULL);
}

static void draw_span(text_style ts, float x, float y, const char *p, const char *end)
{
	const font_t *f = &text_fonts[ts];
	if (!font_sheet[ts])
		return;
	float top = (float)(int)(y - f->mid + 0.5f);
	C2D_Image sheet_img = C2D_SpriteSheetGetImage(font_sheet[ts], 0);
	const Tex3DS_SubTexture *ps = sheet_img.subtex;
	float pen = x;
	unsigned char prev = 0;
	while (*p && (!end || p < end)) {
		unsigned char c = next_char(&p);
		int gi = glyph_of[ts][c];
		if (!gi)
			continue;
		const glyph_t *g = &f->glyphs[gi - 1];
		if (prev)
			pen += kern(f, prev, c);
		prev = c;
		if (c != ' ') {
			Tex3DS_SubTexture sub;
			C2D_Image img;
			sub.width = (u16)g->w;
			sub.height = (u16)f->line;
			sub.left = ps->left + (ps->right - ps->left) * (float)g->x / f->sheet_w;
			sub.right = ps->left + (ps->right - ps->left) * (float)(g->x + g->w) / f->sheet_w;
			sub.top = ps->top + (ps->bottom - ps->top) * (float)g->y / f->sheet_h;
			sub.bottom = ps->top + (ps->bottom - ps->top) * (float)(g->y + f->line) / f->sheet_h;
			img.tex = sheet_img.tex;
			img.subtex = &sub;
			C2D_DrawImageAt(img, (float)(int)(pen + g->off + 0.5f), top, 0.5f, NULL, 1.0f, 1.0f);
		}
		pen += g->adv;
	}
}

void text(text_style ts, float x, float y, align_t a, const char *fmt, ...)
{
	char sz[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(sz, sizeof(sz), fmt, ap);
	va_end(ap);
	if (a == A_CENTER)
		x -= text_w(ts, sz) / 2;
	else if (a == A_RIGHT)
		x -= text_w(ts, sz);
	draw_span(ts, x, y, sz, NULL);
}

float text_wrap(text_style ts, float x, float y, float w, float line_h, const char *s)
{
	while (*s) {
		const char *line = s, *brk = NULL, *p = s;
		while (*p && *p != '\n') {
			if (*p == ' ') {
				if (span_w(ts, line, p) > w)
					break;
				brk = p;
			}
			p++;
		}
		const char *end;
		if (!*p || *p == '\n')
			end = span_w(ts, line, p) > w && brk ? brk : p;
		else
			end = brk ? brk : p;
		draw_span(ts, x, y, line, end);
		y += line_h;
		s = end;
		while (*s == ' ')
			s++;
		if (*s == '\n')
			s++;
	}
	return y;
}

int in_rect(int px, int py, float x, float y, float w, float h)
{
	return px >= x && px < x + w && py >= y && py < y + h;
}
