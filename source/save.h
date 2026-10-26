/* Ur: small binary saves on the SD card (sdmc:/3ds/ur-3ds/), from Gammon-X.
 * Each file has a magic, a version and a checksum; a file that doesn't
 * match (old version, half-written) is treated as absent. */
#ifndef SAVE_H
#define SAVE_H
#include <stddef.h>

#define SAVE_VERSION 1

int save_write(const char *name, const void *data, size_t len);
int save_read(const char *name, void *data, size_t len);     /* 0 on success */
void save_delete(const char *name);
int save_exists(const char *name);

#endif
