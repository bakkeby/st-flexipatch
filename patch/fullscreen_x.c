void
fullscreen(const Arg *arg)
{
	XEvent ev;

	memset(&ev, 0, sizeof(ev));

	ev.xclient.type = ClientMessage;
	ev.xclient.message_type = xw.netwmstate;
	ev.xclient.display = xw.dpy;
	ev.xclient.window = xw.win;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = 2; /* _NET_WM_STATE_TOGGLE */
	ev.xclient.data.l[1] = xw.netwmfullscreen;

	XSendEvent(xw.dpy, DefaultRootWindow(xw.dpy), False, SubstructureNotifyMask|SubstructureRedirectMask, &ev);
}
