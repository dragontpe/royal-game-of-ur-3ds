/* Sound effects and the music loop through NDSP (ported from Gammon-X).
 * Silent, not broken, when the console has no dumped DSP firmware
 * (sdmc:/3ds/dspfirm.cdc). */
#ifndef SFX_H
#define SFX_H

#include <stdbool.h>

typedef enum {
	SFX_CLACK, SFX_TAP, SFX_RATTLE, SFX_THUNK, SFX_TICK, SFX_ROSETTE, SFX_WIN, SFX_LOSE, SFX_COUNT
} sfx_id;

int sfx_init(void);              /* 0 if audio is available */
void sfx_exit(void);
/* pan: -1 left .. +1 right; vol 0..1 */
void sfx_play(sfx_id id, float pan, float vol);
void sfx_enable(bool sounds, bool music);

#endif
