#include <gd.h>

void
setnetwmicon(void)
{
	/* use a png-image to set _NET_WM_ICON */
	FILE* file = fopen(ICON, "r");
	if (file) {
		/* load image in rgba-format */
		const gdImagePtr icon_rgba = gdImageCreateFromPng(file);
		fclose(file);
		/* declare icon-variable which will store the image in bgra-format */
		const int width  = gdImageSX(icon_rgba);
		const int height = gdImageSY(icon_rgba);
		const int icon_n = width * height + 2;
		long icon_bgra[icon_n];
		/* set width and height of the icon */
		int i = 0;
		icon_bgra[i++] = width;
		icon_bgra[i++] = height;
		/* rgba -> bgra */
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				const int pixel_rgba = gdImageGetPixel(icon_rgba, x, y);
				unsigned char *pixel_bgra = (unsigned char *) &icon_bgra[i++];
				pixel_bgra[0] = gdImageBlue(icon_rgba, pixel_rgba);
				pixel_bgra[1] = gdImageGreen(icon_rgba, pixel_rgba);
				pixel_bgra[2] = gdImageRed(icon_rgba, pixel_rgba);
				/* scale alpha from 0-127 to 0-255 */
				const unsigned char alpha = 127 - gdImageAlpha(icon_rgba, pixel_rgba);
				pixel_bgra[3] = alpha == 127 ? 255 : alpha * 2;
			}
		}
		gdImageDestroy(icon_rgba);
		/* set _NET_WM_ICON */
		xw.netwmicon = XInternAtom(xw.dpy, "_NET_WM_ICON", False);
		XChangeProperty(xw.dpy, xw.win, xw.netwmicon, XA_CARDINAL, 32,
		                PropModeReplace, (uchar *) icon_bgra, icon_n);
	}
}
