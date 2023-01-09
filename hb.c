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

hb_font_t *hbfindfont(XftFont *match);

typedef struct {
	XftFont *match;
	hb_font_t *font;
} HbFontMatch;

static int hbfontslen = 0;
static HbFontMatch *hbfontcache = NULL;

/*
 * Poplulate the array with a list of font features, wrapped in FEATURE macro,
 * e. g.
 * FEATURE('c', 'a', 'l', 't'), FEATURE('d', 'l', 'i', 'g')
 */
hb_feature_t features[] = { 0 };

void
hbunloadfonts()
{
	for (int i = 0; i < hbfontslen; i++) {
		hb_font_destroy(hbfontcache[i].font);
		XftUnlockFace(hbfontcache[i].match);
	}

	if (hbfontcache != NULL) {
		free(hbfontcache);
		hbfontcache = NULL;
	}
	hbfontslen = 0;
}

hb_font_t *
hbfindfont(XftFont *match)
{
	for (int i = 0; i < hbfontslen; i++) {
		if (hbfontcache[i].match == match)
			return hbfontcache[i].font;
	}

	/* Font not found in cache, caching it now. */
	hbfontcache = realloc(hbfontcache, sizeof(HbFontMatch) * (hbfontslen + 1));
	FT_Face face = XftLockFace(match);
	hb_font_t *font = hb_ft_font_create(face, NULL);
	if (font == NULL)
		die("Failed to load Harfbuzz font.");

	hbfontcache[hbfontslen].match = match;
	hbfontcache[hbfontslen].font = font;
	hbfontslen += 1;

	return font;
}

void hbtransform(HbTransformData *data, XftFont *xfont, const Glyph *glyphs, int start, int length) {
	Rune rune;
	ushort mode = USHRT_MAX;
	unsigned int glyph_count;
	int i, end = start + length;

	hb_font_t *font = hbfindfont(xfont);
	if (font == NULL)
		return;

	hb_buffer_t *buffer = hb_buffer_create();
	hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);

	/* Fill buffer with codepoints. */
	for (i = start; i < end; i++) {
		rune = glyphs[i].u;
		mode = glyphs[i].mode;
		if (mode & ATTR_WDUMMY)
			rune = 0x0020;
		hb_buffer_add_codepoints(buffer, &rune, 1, 0, 1);
	}

	/* Shape the segment. */
	hb_shape(font, buffer, features, sizeof(features)/sizeof(hb_feature_t));

	/* Get new glyph info. */
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, &glyph_count);

	/** Fill the output. */
	data->buffer = buffer;
	data->glyphs = info;
	data->positions = pos;
	data->count = glyph_count;
}

void hbcleanup(HbTransformData *data) {
	hb_buffer_destroy(data->buffer);
	memset(data, 0, sizeof(HbTransformData));
}
