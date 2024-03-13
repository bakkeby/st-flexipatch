void
setnetwmicon(void)
{
	/* use a farbfeld image to set _NET_WM_ICON */
	FILE* file = fopen(ICON, "r");
	if (file) {
		unsigned char buf[16] = {0};

		int hasdata = fread(buf,1,16,file);
		if (memcmp(buf,"farbfeld",8)) {
			fprintf(stderr,"netwmicon: file %s is not a farbfeld image\n", ICON);
			fclose(file);
			return;
		}

		/* declare icon-variable which will store the image in bgra-format */
		const int width=(buf[8]<<24)|(buf[9]<<16)|(buf[10]<<8)|buf[11];
		const int height=(buf[12]<<24)|(buf[13]<<16)|(buf[14]<<8)|buf[15];
		const int icon_n = width * height + 2;
		long icon_bgra[icon_n];

		/* set width and height of the icon */
		int i = 0;
		icon_bgra[i++] = width;
		icon_bgra[i++] = height;

		/* rgba -> bgra */
		for (int y = 0; y < height && hasdata; y++) {
			for (int x = 0; x < width && hasdata; x++) {
				unsigned char *pixel_bgra = (unsigned char *) &icon_bgra[i++];
				hasdata = fread(buf,1,8,file);
				pixel_bgra[0] = buf[4];
				pixel_bgra[1] = buf[2];
				pixel_bgra[2] = buf[0];
				pixel_bgra[3] = buf[6];
			}
		}

		/* set _NET_WM_ICON */
		xw.netwmicon = XInternAtom(xw.dpy, "_NET_WM_ICON", False);
		XChangeProperty(xw.dpy, xw.win, xw.netwmicon, XA_CARDINAL, 32,
				PropModeReplace, (uchar *) icon_bgra, icon_n);

		fclose(file);
	}
}
