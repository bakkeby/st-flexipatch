#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>
#include <hb.h>
#include <hb-ft.h>

#include "st.h"
#include "hb.h"

#define FEATURE(c1,c2,c3,c4) { .tag = HB_TAG(c1,c2,c3,c4), .value = 1, .start = HB_FEATURE_GLOBAL_START, .end = HB_FEATURE_GLOBAL_END }
#define BUFFER_STEP 256

hb_font_t *hbfindfont(XftFont *match);

typedef struct {
	XftFont *match;
	hb_font_t *font;
} HbFontMatch;

typedef struct {
	size_t capacity;
	HbFontMatch *fonts;
} HbFontCache;

static HbFontCache hbfontcache = { 0, NULL };

typedef struct {
	size_t capacity;
	Rune *runes;
} RuneBuffer;

static RuneBuffer hbrunebuffer = { 0, NULL };
static hb_buffer_t *hbbuffer;

/*
 * Poplulate the array with a list of font features, wrapped in FEATURE macro,
 * e. g.
 * FEATURE('c', 'a', 'l', 't'), FEATURE('d', 'l', 'i', 'g')
 */
hb_feature_t features[] = { };

void
hbcreatebuffer(void)
{
	hbbuffer = hb_buffer_create();
}

void
hbdestroybuffer(void)
{
	hb_buffer_destroy(hbbuffer);
}

void
hbunloadfonts(void)
{
	for (int i = 0; i < hbfontcache.capacity; i++) {
		hb_font_destroy(hbfontcache.fonts[i].font);
		XftUnlockFace(hbfontcache.fonts[i].match);
	}

	if (hbfontcache.fonts != NULL) {
		free(hbfontcache.fonts);
		hbfontcache.fonts = NULL;
	}
	hbfontcache.capacity = 0;
}

hb_font_t *
hbfindfont(XftFont *match)
{
	for (int i = 0; i < hbfontcache.capacity; i++) {
		if (hbfontcache.fonts[i].match == match)
			return hbfontcache.fonts[i].font;
	}

	/* Font not found in cache, caching it now. */
	hbfontcache.fonts = realloc(hbfontcache.fonts, sizeof(HbFontMatch) * (hbfontcache.capacity + 1));
	FT_Face face = XftLockFace(match);
	hb_font_t *font = hb_ft_font_create(face, NULL);
	if (font == NULL)
		die("Failed to load Harfbuzz font.");

	hbfontcache.fonts[hbfontcache.capacity].match = match;
	hbfontcache.fonts[hbfontcache.capacity].font = font;
	hbfontcache.capacity += 1;

	return font;
}

void
hbtransform(HbTransformData *data, XftFont *xfont, const Glyph *glyphs, int start, int length)
{
	uint32_t mode;
	unsigned int glyph_count;
	int rune_idx, glyph_idx, end = start + length;
	hb_buffer_t *buffer = hbbuffer;

	hb_font_t *font = hbfindfont(xfont);
	if (font == NULL) {
		data->count = 0;
		return;
	}

	hb_buffer_reset(buffer);
	hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
	hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);

	/* Resize the buffer if required length is larger. */
	if (hbrunebuffer.capacity < length) {
		hbrunebuffer.capacity = (length / BUFFER_STEP + 1) * BUFFER_STEP;
		hbrunebuffer.runes = realloc(hbrunebuffer.runes, hbrunebuffer.capacity * sizeof(Rune));
	}

	/* Fill buffer with codepoints. */
	for (rune_idx = 0, glyph_idx = start; glyph_idx < end; glyph_idx++, rune_idx++) {
		hbrunebuffer.runes[rune_idx] = glyphs[glyph_idx].u;
		mode = glyphs[glyph_idx].mode;
		if (mode & ATTR_WDUMMY)
			hbrunebuffer.runes[rune_idx] = 0x0020;
	}
	hb_buffer_add_codepoints(buffer, hbrunebuffer.runes, length, 0, length);

	/* Shape the segment. */
	hb_shape(font, buffer, features, sizeof(features)/sizeof(hb_feature_t));

	/* Get new glyph info. */
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, &glyph_count);

	/* Fill the output. */
	data->buffer = buffer;
	data->glyphs = info;
	data->positions = pos;
	data->count = glyph_count;
}
