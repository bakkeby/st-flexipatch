void
setnetwmicon(void)
{
	xw.netwmicon = XInternAtom(xw.dpy, "_NET_WM_ICON", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmicon, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)&icon, LEN(icon));
}
