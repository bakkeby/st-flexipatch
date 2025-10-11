static Window embed;

void
createnotify(XEvent *e)
{
	XWindowChanges wc;

	if (embed || e->xcreatewindow.override_redirect)
		return;

	embed = e->xcreatewindow.window;

	XReparentWindow(xw.dpy, embed, xw.win, 0, 0);
	XSelectInput(xw.dpy, embed, PropertyChangeMask | StructureNotifyMask | EnterWindowMask);

	XMapWindow(xw.dpy, embed);
	sendxembed(XEMBED_EMBEDDED_NOTIFY, 0, xw.win, 0);

	wc.width = win.w;
	wc.height = win.h;
	XConfigureWindow(xw.dpy, embed, CWWidth | CWHeight, &wc);

	XSetInputFocus(xw.dpy, embed, RevertToParent, CurrentTime);
}

void
destroynotify(XEvent *e)
{
	visibility(e);
	if (embed == e->xdestroywindow.window) {
		focus(e);
	}
}

void
sendxembed(long msg, long detail, long d1, long d2)
{
	XEvent e = { 0 };

	e.xclient.window = embed;
	e.xclient.type = ClientMessage;
	e.xclient.message_type = xw.xembed;
	e.xclient.format = 32;
	e.xclient.data.l[0] = CurrentTime;
	e.xclient.data.l[1] = msg;
	e.xclient.data.l[2] = detail;
	e.xclient.data.l[3] = d1;
	e.xclient.data.l[4] = d2;
	XSendEvent(xw.dpy, embed, False, NoEventMask, &e);
}
