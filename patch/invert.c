static int invertcolors = 0;

void
invert(const Arg *dummy)
{
	invertcolors = !invertcolors;
	redraw();
}

Color
invertedcolor(Color *clr)
{
	XRenderColor rc;
	Color inverted;
	rc.red = ~clr->color.red;
	rc.green = ~clr->color.green;
	rc.blue = ~clr->color.blue;
	rc.alpha = clr->color.alpha;
	XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &rc, &inverted);
	return inverted;
}