#include <X11/Xft/Xft.h>
#include <hb.h>
#include <hb-ft.h>

typedef struct {
	hb_buffer_t *buffer;
	hb_glyph_info_t *glyphs;
	hb_glyph_position_t *positions;
	unsigned int count;
} HbTransformData;

void hbcreatebuffer(void);
void hbdestroybuffer(void);
void hbunloadfonts(void);
void hbtransform(HbTransformData *, XftFont *, const Glyph *, int, int);
