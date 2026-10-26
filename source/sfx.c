/* Sound effects and music. See sfx.h. Clips are raw 16-bit mono PCM at
 * 22050 Hz, made by tools/audio/make_sfx.py. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sfx.h"

#define RATE 22050
#define NCHAN 6          /* channels 0..5 for effects */
#define MUSIC_CHAN 6
#define MAXVAR 3

typedef struct {
	s16 *data;
	u32 samples;
} clip_t;

static const char *const files[SFX_COUNT][MAXVAR] = {
	{ "clack0", "clack1", "clack2" },
	{ "tap0", "tap1", NULL },
	{ "rattle0", "rattle1", NULL },
	{ "thunk", NULL, NULL },
	{ "tick", NULL, NULL },
	{ "rosette", NULL, NULL },
	{ "win", NULL, NULL },
	{ "lose", NULL, NULL },
};

static clip_t clips[SFX_COUNT][MAXVAR], music;
static int nvar[SFX_COUNT];
static ndspWaveBuf wbuf[NCHAN], mbuf;
static int ready, next_chan;
static bool sounds_on = true, music_on = true, music_playing;

static int load(const char *name, clip_t *c)
{
	char path[64];
	snprintf(path, sizeof(path), "romfs:/sfx/%s.pcm", name);
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	rewind(f);
	c->data = linearAlloc((size_t)len);
	if (!c->data || fread(c->data, 1, (size_t)len, f) != (size_t)len) {
		fclose(f);
		return -1;
	}
	fclose(f);
	c->samples = (u32)len / 2;
	DSP_FlushDataCache(c->data, (u32)len);
	return 0;
}

static void music_update(void)
{
	if (!ready || !music.data)
		return;
	if (music_on && !music_playing) {
		float mix[12] = { 0.55f, 0.55f };
		ndspChnWaveBufClear(MUSIC_CHAN);
		ndspChnSetMix(MUSIC_CHAN, mix);
		memset(&mbuf, 0, sizeof(mbuf));
		mbuf.data_vaddr = music.data;
		mbuf.nsamples = music.samples;
		mbuf.looping = true;
		ndspChnWaveBufAdd(MUSIC_CHAN, &mbuf);
		music_playing = true;
	} else if (!music_on && music_playing) {
		ndspChnWaveBufClear(MUSIC_CHAN);
		music_playing = false;
	}
}

int sfx_init(void)
{
	if (R_FAILED(ndspInit()))
		return -1;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	for (int i = 0; i <= MUSIC_CHAN; i++) {
		ndspChnReset(i);
		ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
		ndspChnSetRate(i, RATE);
		ndspChnSetFormat(i, NDSP_FORMAT_MONO_PCM16);
	}
	for (int i = 0; i < SFX_COUNT; i++)
		for (int k = 0; k < MAXVAR && files[i][k]; k++)
			if (load(files[i][k], &clips[i][nvar[i]]) == 0)
				nvar[i]++;
	load("music", &music);
	ready = 1;
	music_update();
	return 0;
}

void sfx_exit(void)
{
	if (!ready)
		return;
	ndspExit();
	for (int i = 0; i < SFX_COUNT; i++)
		for (int k = 0; k < nvar[i]; k++)
			linearFree(clips[i][k].data);
	if (music.data)
		linearFree(music.data);
	ready = 0;
}

void sfx_enable(bool sounds, bool mus)
{
	sounds_on = sounds;
	music_on = mus;
	music_update();
}

void sfx_play(sfx_id id, float pan, float vol)
{
	if (!ready || !sounds_on || id >= SFX_COUNT || !nvar[id])
		return;
	clip_t *c = &clips[id][nvar[id] > 1 ? rand() % nvar[id] : 0];
	int ch = next_chan;
	next_chan = (next_chan + 1) % NCHAN;
	if (pan < -1)
		pan = -1;
	if (pan > 1)
		pan = 1;
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = vol * (pan <= 0 ? 1.0f : 1.0f - pan * 0.6f);
	mix[1] = vol * (pan >= 0 ? 1.0f : 1.0f + pan * 0.6f);
	ndspChnWaveBufClear(ch);
	ndspChnSetMix(ch, mix);
	memset(&wbuf[ch], 0, sizeof(wbuf[ch]));
	wbuf[ch].data_vaddr = c->data;
	wbuf[ch].nsamples = c->samples;
	ndspChnWaveBufAdd(ch, &wbuf[ch]);
}
