/* Ur: SD card saves (ported from Gammon-X). See save.h. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "save.h"

#if defined(UR_AUTOSHOT)
#define DIR "sdmc:/3ds/ur-3ds/autoshot"      /* never touch real saves */
#elif defined(UR_FUZZ)
#define DIR "sdmc:/3ds/ur-3ds/fuzz"
#else
#define DIR "sdmc:/3ds/ur-3ds"
#endif
#define MAGIC 0x56535255u      /* "URSV" */

typedef struct {
    u32 magic, version, len, sum;
} header_t;

static u32
checksum(const void *data, size_t len)
{
    const u8 *p = data;
    u32 h = 2166136261u;            /* FNV-1a */
    size_t i;
    for (i = 0; i < len; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static void
path_of(char *out, size_t n, const char *name, const char *ext)
{
    snprintf(out, n, DIR "/%s%s", name, ext);
}

int
save_write(const char *name, const void *data, size_t len)
{
    char tmp[96], final[96];
    header_t h = { MAGIC, SAVE_VERSION, (u32) len, checksum(data, len) };
    FILE *f;
    int ok;

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ur-3ds", 0777);
    mkdir(DIR, 0777);
    path_of(tmp, sizeof(tmp), name, ".tmp");
    path_of(final, sizeof(final), name, ".sav");
    if (!(f = fopen(tmp, "wb")))
        return -1;
    ok = fwrite(&h, sizeof(h), 1, f) == 1 && fwrite(data, 1, len, f) == len;
    ok &= fclose(f) == 0;
    if (!ok) {
        remove(tmp);
        return -1;
    }
    remove(final);                  /* FAT rename won't overwrite */
    return rename(tmp, final) == 0 ? 0 : -1;
}

static int
read_file(const char *path, void *data, size_t len)
{
    header_t h;
    FILE *f;
    int ok;

    if (!(f = fopen(path, "rb")))
        return -1;
    ok = fread(&h, sizeof(h), 1, f) == 1 && h.magic == MAGIC && h.version == SAVE_VERSION &&
        h.len == len && fread(data, 1, len, f) == len && checksum(data, len) == h.sum;
    fclose(f);
    return ok ? 0 : -1;
}

int
save_read(const char *name, void *data, size_t len)
{
    char path[96];

    path_of(path, sizeof(path), name, ".sav");
    if (read_file(path, data, len) == 0)
        return 0;
    /* power lost between removing the old save and renaming the new one */
    path_of(path, sizeof(path), name, ".tmp");
    return read_file(path, data, len);
}

void
save_delete(const char *name)
{
    char path[96];
    path_of(path, sizeof(path), name, ".sav");
    remove(path);
}

int
save_exists(const char *name)
{
    char path[96];
    struct stat st;
    path_of(path, sizeof(path), name, ".sav");
    return stat(path, &st) == 0;
}
