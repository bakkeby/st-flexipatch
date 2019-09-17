int
isvbellcell(int x, int y)
{
	if (vbellmode == 1)
		return 1;
	if (vbellmode == 2)
		return y == 0 || y == win.th / win.ch - 1 ||
		       x == 0 || x == win.tw / win.cw - 1;
	return 0;
}

void
vbellbegin()
{
	clock_gettime(CLOCK_MONOTONIC, &win.lastvbell);
	if (win.vbellset) /* already visible, just extend win.lastvbell */
		return;
	win.vbellset = 1;
	#if VISUALBELL_3_PATCH
	if (vbellmode != 3) /* 3 is an overlay, no need to re-render cells */
		tfulldirt();
	#else
	tfulldirt();
	#endif // VISUALBELL_3_PATCH
	draw();
	XFlush(xw.dpy);
}

#if VISUALBELL_3_PATCH
void
xfillcircle(int x, int y, int r, uint color_ix)
{
	XSetForeground(xw.dpy, dc.gc, dc.col[color_ix].pixel);
	XFillArc(xw.dpy, xw.buf, dc.gc, x - r, y - r, r * 2, r * 2, 0, 360*64);
}

void
xdrawvbell() {
	int r = round(vbellradius * (vbellradius > 0 ? win.w : -win.cw));
	int x = borderpx + r + vbellx * (win.tw - 2 * r);
	int y = borderpx + r + vbelly * (win.th - 2 * r);
	xfillcircle(x, y, r, vbellcolor_outline);
	xfillcircle(x, y, r / 1.2, vbellcolor); /* 1.2 - an artistic choice */
}
#endif // VISUALBELL_3_PATCH