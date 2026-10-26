#ifndef UR_PAGES_H
#define UR_PAGES_H

#include <3ds.h>
#include <stdbool.h>

enum { PAGES_RULES, PAGES_STORY };

void pages_open(int which);
/* 1 when the reader wants to leave */
int pages_update(u32 down, bool touch, int tx, int ty);
void pages_draw(void);

#endif
