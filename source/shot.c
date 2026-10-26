/* Ur: framebuffer screenshots (from Gammon-X). See shot.h. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "shot.h"

#define W 400
#define H 480

static void
put32(u8 *p, u32 v)
{
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

/* Framebuffers are 240 px wide columns, rotated: pixel (x, y) lives at
 * (x * 240 + 239 - y), 3 bytes BGR. */
static void
copy_screen(u8 *img, gfxScreen_t scr, int ox, int oy, int sw)
{
    u16 fw, fh;
    u8 *fb = gfxGetFramebuffer(scr, GFX_LEFT, &fw, &fh);
    int x, y;

    for (y = 0; y < 240; y++)
        for (x = 0; x < sw; x++) {
            const u8 *s = fb + (x * 240 + 239 - y) * 3;
            u8 *d = img + ((H - 1 - (oy + y)) * W + ox + x) * 3;   /* BMP rows go bottom-up */
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
}

int
shot_save(const char *name)
{
    static u8 img[W * H * 3];
    u8 hdr[54] = { 'B', 'M' };
    char path[96];
    FILE *f;

    memset(img, 0x22, sizeof(img));
    copy_screen(img, GFX_TOP, 0, 0, 400);
    copy_screen(img, GFX_BOTTOM, 40, 240, 320);

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ur-3ds", 0777);
    mkdir("sdmc:/3ds/ur-3ds/shots", 0777);
    snprintf(path, sizeof(path), "sdmc:/3ds/ur-3ds/shots/%s.bmp", name);
    if (!(f = fopen(path, "wb")))
        return -1;
    put32(hdr + 2, 54 + sizeof(img));
    put32(hdr + 10, 54);
    put32(hdr + 14, 40);
    put32(hdr + 18, W);
    put32(hdr + 22, H);
    hdr[26] = 1;
    hdr[28] = 24;
    put32(hdr + 34, sizeof(img));
    fwrite(hdr, 1, 54, f);
    fwrite(img, 1, sizeof(img), f);
    fclose(f);
    return 0;
}
