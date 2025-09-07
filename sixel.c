// sixel.c (part of mintty)
// originally written by kmiya@cluti (https://github.com/saitoha/sixel/blob/master/fromsixel.c)
// Licensed under the terms of the GNU General Public License v3 or later.

#include <stdlib.h>
#include <string.h>  /* memcpy */

#include "st.h"
#include "win.h"
#include "sixel.h"
#include "sixel_hls.h"

#define SIXEL_RGB(r, g, b) ((255 << 24) + ((r) << 16) + ((g) << 8) +  (b))
#define SIXEL_PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))
#define SIXEL_XRGB(r,g,b) SIXEL_RGB(SIXEL_PALVAL(r, 255, 100), SIXEL_PALVAL(g, 255, 100), SIXEL_PALVAL(b, 255, 100))

static sixel_color_t const sixel_default_color_table[] = {
	SIXEL_XRGB( 0,  0,  0),  /*  0 Black    */
	SIXEL_XRGB(20, 20, 80),  /*  1 Blue     */
	SIXEL_XRGB(80, 13, 13),  /*  2 Red      */
	SIXEL_XRGB(20, 80, 20),  /*  3 Green    */
	SIXEL_XRGB(80, 20, 80),  /*  4 Magenta  */
	SIXEL_XRGB(20, 80, 80),  /*  5 Cyan     */
	SIXEL_XRGB(80, 80, 20),  /*  6 Yellow   */
	SIXEL_XRGB(53, 53, 53),  /*  7 Gray 50% */
	SIXEL_XRGB(26, 26, 26),  /*  8 Gray 25% */
	SIXEL_XRGB(33, 33, 60),  /*  9 Blue*    */
	SIXEL_XRGB(60, 26, 26),  /* 10 Red*     */
	SIXEL_XRGB(33, 60, 33),  /* 11 Green*   */
	SIXEL_XRGB(60, 33, 60),  /* 12 Magenta* */
	SIXEL_XRGB(33, 60, 60),  /* 13 Cyan*    */
	SIXEL_XRGB(60, 60, 33),  /* 14 Yellow*  */
	SIXEL_XRGB(80, 80, 80),  /* 15 Gray 75% */
};

void
scroll_images(int n) {
	ImageList *im, *next;
	#if SCROLLBACK_PATCH || REFLOW_PATCH
	int top = tisaltscr() ? 0 : term.scr - HISTSIZE;
	#else
	int top = 0;
	#endif // SCROLLBACK_PATCH

	for (im = term.images; im; im = next) {
		next = im->next;
		im->y += n;

		/* check if the current sixel has exceeded the maximum
		 * draw distance, and should therefore be deleted */
		if (im->y < top) {
			//fprintf(stderr, "im@0x%08x exceeded maximum distance\n");
			delete_image(im);
		}
	}
}

void
delete_image(ImageList *im)
{
	if (im->prev)
		im->prev->next = im->next;
	else
		term.images = im->next;
	if (im->next)
		im->next->prev = im->prev;
	if (im->pixmap)
		XFreePixmap(xw.dpy, (Drawable)im->pixmap);
	if (im->clipmask)
		XFreePixmap(xw.dpy, (Drawable)im->clipmask);
	free(im->pixels);
	free(im);
}

static int
set_default_color(sixel_image_t *image)
{
	int i;
	int n;
	int r;
	int g;
	int b;

	/* palette initialization */
	for (n = 1; n < 17; n++) {
		image->palette[n] = sixel_default_color_table[n - 1];
	}

	/* colors 17-232 are a 6x6x6 color cube */
	for (r = 0; r < 6; r++) {
		for (g = 0; g < 6; g++) {
			for (b = 0; b < 6; b++) {
				image->palette[n++] = SIXEL_RGB(r * 51, g * 51, b * 51);
			}
		}
	}

	/* colors 233-256 are a grayscale ramp, intentionally leaving out */
	for (i = 0; i < 24; i++) {
		image->palette[n++] = SIXEL_RGB(i * 11, i * 11, i * 11);
	}

	/* sixels rarely use more than 256 colors and if they do, they use a custom
	 * palette, so we don't need to initialize these colors */
	/*
	for (; n < DECSIXEL_PALETTE_MAX; n++) {
		image->palette[n] = SIXEL_RGB(255, 255, 255);
	}
	*/

	return (0);
}

static int
sixel_image_init(
    sixel_image_t    *image,
    int              width,
    int              height,
    int              fgcolor,
    int              bgcolor,
    int              use_private_register)
{
	int status = (-1);
	size_t size;

	size = (size_t)(width * height) * sizeof(sixel_color_no_t);
	image->width = width;
	image->height = height;
	image->data = (sixel_color_no_t *)malloc(size);
	image->ncolors = 2;
	image->use_private_register = use_private_register;

	if (image->data == NULL) {
		status = (-1);
		goto end;
	}
	memset(image->data, 0, size);

	image->palette[0] = bgcolor;

	if (image->use_private_register)
		image->palette[1] = fgcolor;

	image->palette_modified = 0;

	status = (0);

end:
	return status;
}


static int
image_buffer_resize(
    sixel_image_t   *image,
    int              width,
    int              height)
{
	int status = (-1);
	size_t size;
	sixel_color_no_t *alt_buffer;
	int n;
	int min_height;

	size = (size_t)(width * height) * sizeof(sixel_color_no_t);
	alt_buffer = (sixel_color_no_t *)malloc(size);
	if (alt_buffer == NULL) {
		/* free source image */
		free(image->data);
		image->data = NULL;
		status = (-1);
		goto end;
	}

	min_height = height > image->height ? image->height: height;
	if (width > image->width) {  /* if width is extended */
		for (n = 0; n < min_height; ++n) {
			/* copy from source image */
			memcpy(alt_buffer + width * n,
			       image->data + image->width * n,
			       (size_t)image->width * sizeof(sixel_color_no_t));
			/* fill extended area with background color */
			memset(alt_buffer + width * n + image->width,
			       0,
			       (size_t)(width - image->width) * sizeof(sixel_color_no_t));
		}
	} else {
		for (n = 0; n < min_height; ++n) {
			/* copy from source image */
			memcpy(alt_buffer + width * n,
			       image->data + image->width * n,
			       (size_t)width * sizeof(sixel_color_no_t));
		}
	}

	if (height > image->height) {  /* if height is extended */
		/* fill extended area with background color */
		memset(alt_buffer + width * image->height,
		       0,
		       (size_t)(width * (height - image->height)) * sizeof(sixel_color_no_t));
	}

	/* free source image */
	free(image->data);

	image->data = alt_buffer;
	image->width = width;
	image->height = height;

	status = (0);

end:
	return status;
}

static void
sixel_image_deinit(sixel_image_t *image)
{
	if (image->data)
		free(image->data);
	image->data = NULL;
}

int
sixel_parser_init(sixel_state_t *st,
                  int transparent,
                  sixel_color_t fgcolor, sixel_color_t bgcolor,
                  unsigned char use_private_register,
                  int cell_width, int cell_height)
{
	int status = (-1);

	st->state = PS_DECSIXEL;
	st->pos_x = 0;
	st->pos_y = 0;
	st->max_x = 0;
	st->max_y = 0;
	st->attributed_pan = 2;
	st->attributed_pad = 1;
	st->attributed_ph = 0;
	st->attributed_pv = 0;
	st->transparent = transparent;
	st->repeat_count = 1;
	st->color_index = 16;
	st->grid_width = cell_width;
	st->grid_height = cell_height;
	st->nparams = 0;
	st->param = 0;

	/* buffer initialization */
	status = sixel_image_init(&st->image, 1, 1, fgcolor, transparent ? 0 : bgcolor, use_private_register);

	return status;
}

int
sixel_parser_set_default_color(sixel_state_t *st)
{
	return set_default_color(&st->image);
}

int
sixel_parser_finalize(sixel_state_t *st, ImageList **newimages, int cx, int cy, int cw, int ch)
{
	sixel_image_t *image = &st->image;
	int x, y;
	sixel_color_no_t *src;
	sixel_color_t *dst, color;
	int w, h;
	int i, j, cols, numimages;
	char trans;
	ImageList *im, *next, *tail;

	if (!image->data)
		return -1;

	if (++st->max_x < st->attributed_ph)
		st->max_x = st->attributed_ph;

	if (++st->max_y < st->attributed_pv)
		st->max_y = st->attributed_pv;

	if (image->use_private_register && image->ncolors > 2 && !image->palette_modified) {
		if (set_default_color(image) < 0)
			return -1;
	}

	w = MIN(st->max_x, image->width);
	h = MIN(st->max_y, image->height);

	if ((numimages = (h + ch-1) / ch) <= 0)
		return -1;

	cols = (w + cw-1) / cw;

	*newimages = NULL, tail = NULL;
	for (y = 0, i = 0; i < numimages; i++) {
		if ((im = malloc(sizeof(ImageList)))) {
			if (!tail) {
				*newimages = tail = im;
				im->prev = im->next = NULL;
			} else {
				tail->next = im;
				im->prev = tail;
				im->next = NULL;
				tail = im;
			}
			im->x = cx;
			im->y = cy + i;
			im->cols = cols;
			im->width = w;
			im->height = MIN(h - ch * i, ch);
			im->pixels = malloc(im->width * im->height * 4);
			im->pixmap = NULL;
			im->clipmask = NULL;
			im->cw = cw;
			im->ch = ch;
		}
		if (!im || !im->pixels) {
			for (im = *newimages; im; im = next) {
				next = im->next;
				if (im->pixels)
					free(im->pixels);
				free(im);
			}
			*newimages = NULL;
			return -1;
		}
		dst = (sixel_color_t *)im->pixels;
		for (trans = 0, j = 0; j < im->height && y < h; j++, y++) {
			src = st->image.data + image->width * y;
			for (x = 0; x < w; x++) {
				color = st->image.palette[*src++];
				trans |= (color == 0);
				*dst++ = color;
			}
		}
		im->transparent = (st->transparent && trans);
	}

	return numimages;
}

/* convert sixel data into indexed pixel bytes and palette data */
int
sixel_parser_parse(sixel_state_t *st, const unsigned char *p, size_t len)
{
	int n = 0;
	int i;
	int x;
	int y;
	int bits;
	int sx;
	int sy;
	int c;
	int pos;
	int width;
	const unsigned char *p0 = p, *p2 = p + len;
	sixel_image_t *image = &st->image;
	sixel_color_no_t *data, color_index;

	if (!image->data)
		st->state = PS_ERROR;

	while (p < p2) {
		switch (st->state) {
		case PS_ESC:
			goto end;

		case PS_DECSIXEL:
			switch (*p) {
			case '\x1b':
				st->state = PS_ESC;
				break;
			case '"':
				st->param = 0;
				st->nparams = 0;
				st->state = PS_DECGRA;
				p++;
				break;
			case '!':
				st->param = 0;
				st->nparams = 0;
				st->state = PS_DECGRI;
				p++;
				break;
			case '#':
				st->param = 0;
				st->nparams = 0;
				st->state = PS_DECGCI;
				p++;
				break;
			case '$':
				/* DECGCR Graphics Carriage Return */
				st->pos_x = 0;
				p++;
				break;
			case '-':
				/* DECGNL Graphics Next Line */
				st->pos_x = 0;
				if (st->pos_y < DECSIXEL_HEIGHT_MAX - 5 - 6)
					st->pos_y += 6;
				else
					st->pos_y = DECSIXEL_HEIGHT_MAX + 1;
				p++;
				break;
			default:
				if (*p >= '?' && *p <= '~') {  /* sixel characters */
					if ((image->width < (st->pos_x + st->repeat_count) || image->height < (st->pos_y + 6))
					        && image->width < DECSIXEL_WIDTH_MAX && image->height < DECSIXEL_HEIGHT_MAX) {
						sx = image->width * 2;
						sy = image->height * 2;
						while (sx < (st->pos_x + st->repeat_count) || sy < (st->pos_y + 6)) {
							sx *= 2;
							sy *= 2;
						}

						sx = MIN(sx, DECSIXEL_WIDTH_MAX);
						sy = MIN(sy, DECSIXEL_HEIGHT_MAX);

						if (image_buffer_resize(image, sx, sy) < 0) {
							perror("sixel_parser_parse() failed");
							st->state = PS_ERROR;
							p++;
							break;
						}
					}

					if (st->color_index > image->ncolors)
						image->ncolors = st->color_index;

					if (st->pos_x + st->repeat_count > image->width)
						st->repeat_count = image->width - st->pos_x;

					if (st->repeat_count > 0 && st->pos_y + 5 < image->height) {
						bits = *p - '?';
						if (bits != 0) {
							data = image->data + image->width * st->pos_y + st->pos_x;
							width = image->width;
							color_index = st->color_index;
							if (st->repeat_count <= 1) {
								if (bits & 0x01)
									*data = color_index, n = 0;
								data += width;
								if (bits & 0x02)
									*data = color_index, n = 1;
								data += width;
								if (bits & 0x04)
									*data = color_index, n = 2;
								data += width;
								if (bits & 0x08)
									*data = color_index, n = 3;
								data += width;
								if (bits & 0x10)
									*data = color_index, n = 4;
								if (bits & 0x20)
									data[width] = color_index, n = 5;
								if (st->max_x < st->pos_x)
									st->max_x = st->pos_x;
							} else {
								/* st->repeat_count > 1 */
								for (i = 0; bits; bits >>= 1, i++, data += width) {
									if (bits & 1) {
										data[0] = color_index;
										data[1] = color_index;
										for (x = 2; x < st->repeat_count; x++)
											data[x] = color_index;
										n = i;
									}
								}
								if (st->max_x < (st->pos_x + st->repeat_count - 1))
									st->max_x = st->pos_x + st->repeat_count - 1;
							}
							if (st->max_y < (st->pos_y + n))
								st->max_y = st->pos_y + n;
						}
					}
					if (st->repeat_count > 0)
						st->pos_x += st->repeat_count;
					st->repeat_count = 1;
				}
				p++;
				break;
			}
			break;

		case PS_DECGRA:
			/* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
			switch (*p) {
			case '\x1b':
				st->state = PS_ESC;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				st->param = st->param * 10 + *p - '0';
				st->param = MIN(st->param, DECSIXEL_PARAMVALUE_MAX);
				p++;
				break;
			case ';':
				if (st->nparams < DECSIXEL_PARAMS_MAX)
					st->params[st->nparams++] = st->param;
				st->param = 0;
				p++;
				break;
			default:
				if (st->nparams < DECSIXEL_PARAMS_MAX)
					st->params[st->nparams++] = st->param;
				if (st->nparams > 0)
					st->attributed_pad = st->params[0];
				if (st->nparams > 1)
					st->attributed_pan = st->params[1];
				if (st->nparams > 2 && st->params[2] > 0)
					st->attributed_ph = st->params[2];
				if (st->nparams > 3 && st->params[3] > 0)
					st->attributed_pv = st->params[3];

				if (st->attributed_pan <= 0)
					st->attributed_pan = 1;
				if (st->attributed_pad <= 0)
					st->attributed_pad = 1;

				if (image->width < st->attributed_ph ||
				        image->height < st->attributed_pv) {
					sx = MAX(image->width, st->attributed_ph);
					sy = MAX(image->height, st->attributed_pv);

					/* the height of the image buffer must be divisible by 6
					 * to avoid unnecessary resizing of the image buffer when
					 * parsing the last sixel line */
					sy = (sy + 5) / 6 * 6;

					sx = MIN(sx, DECSIXEL_WIDTH_MAX);
					sy = MIN(sy, DECSIXEL_HEIGHT_MAX);

					if (image_buffer_resize(image, sx, sy) < 0) {
						perror("sixel_parser_parse() failed");
						st->state = PS_ERROR;
						break;
					}
				}
				st->state = PS_DECSIXEL;
				st->param = 0;
				st->nparams = 0;
			}
			break;

		case PS_DECGRI:
			/* DECGRI Graphics Repeat Introducer ! Pn Ch */
			switch (*p) {
			case '\x1b':
				st->state = PS_ESC;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				st->param = st->param * 10 + *p - '0';
				st->param = MIN(st->param, DECSIXEL_PARAMVALUE_MAX);
				p++;
				break;
			default:
				st->repeat_count = MAX(st->param, 1);
				st->state = PS_DECSIXEL;
				st->param = 0;
				st->nparams = 0;
				break;
			}
			break;

		case PS_DECGCI:
			/* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
			switch (*p) {
			case '\x1b':
				st->state = PS_ESC;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				st->param = st->param * 10 + *p - '0';
				st->param = MIN(st->param, DECSIXEL_PARAMVALUE_MAX);
				p++;
				break;
			case ';':
				if (st->nparams < DECSIXEL_PARAMS_MAX)
					st->params[st->nparams++] = st->param;
				st->param = 0;
				p++;
				break;
			default:
				st->state = PS_DECSIXEL;
				if (st->nparams < DECSIXEL_PARAMS_MAX)
					st->params[st->nparams++] = st->param;
				st->param = 0;

				if (st->nparams > 0) {
					st->color_index = st->params[0];
					if (st->color_index < 0)
						st->color_index = 0;
					else if (st->color_index >= DECSIXEL_PALETTE_MAX)
						st->color_index = DECSIXEL_PALETTE_MAX - 1;
					st->color_index++; /* offset by 1 (background) */
				}

				if (st->nparams > 4) {
					st->image.palette_modified = 1;
					if (st->params[1] == 1) {
						/* HLS */
						st->params[2] = MIN(st->params[2], 360);
						st->params[3] = MIN(st->params[3], 100);
						st->params[4] = MIN(st->params[4], 100);
						image->palette[st->color_index]
						    = hls_to_rgb(st->params[2], st->params[3], st->params[4]);
					} else if (st->params[1] == 2) {
						/* RGB */
						st->params[2] = MIN(st->params[2], 100);
						st->params[3] = MIN(st->params[3], 100);
						st->params[4] = MIN(st->params[4], 100);
						image->palette[st->color_index]
						    = SIXEL_XRGB(st->params[2], st->params[3], st->params[4]);
					}
				}
				break;
			}
			break;

		case PS_ERROR:
			if (*p == '\x1b') {
				st->state = PS_ESC;
				goto end;
			}
			p++;
			break;
		default:
			break;
		}
	}

end:
	return p - p0;
}

void
sixel_parser_deinit(sixel_state_t *st)
{
	if (st)
		sixel_image_deinit(&st->image);
}

Pixmap
sixel_create_clipmask(char *pixels, int width, int height)
{
	char c, *clipdata, *dst;
	int b, i, n, y, w;
	int msb = (XBitmapBitOrder(xw.dpy) == MSBFirst);
	sixel_color_t *src = (sixel_color_t *)pixels;
	Pixmap clipmask;

	clipdata = dst = malloc((width+7)/8 * height);
	if (!clipdata)
		return (Pixmap)None;

	for (y = 0; y < height; y++) {
		for (w = width; w > 0; w -= n) {
			n = MIN(w, 8);
			if (msb) {
				for (b = 0x80, c = 0, i = 0; i < n; i++, b >>= 1)
					c |= (*src++) ? b : 0;
			} else {
				for (b = 0x01, c = 0, i = 0; i < n; i++, b <<= 1)
					c |= (*src++) ? b : 0;
			}
			*dst++ = c;
		}
	}

	clipmask = XCreateBitmapFromData(xw.dpy, xw.win, clipdata, width, height);
	free(clipdata);
	return clipmask;
}
