/* Ur: save both screens as one BMP on the SD card. */
#ifndef SHOT_H
#define SHOT_H
/* Writes sdmc:/3ds/ur-3ds/shots/<name>.bmp (top screen above bottom).
 * The picture must have been static for 2+ frames (double buffering). */
int shot_save(const char *name);
#endif
