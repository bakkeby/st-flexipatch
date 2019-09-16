void
ximopen(Display *dpy)
{
	XIMCallback destroy = { .client_data = NULL, .callback = ximdestroy };

	if ((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL) {
		XSetLocaleModifiers("@im=local");
		if ((xw.xim =  XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL) {
			XSetLocaleModifiers("@im=");
			if ((xw.xim = XOpenIM(xw.dpy,
					NULL, NULL, NULL)) == NULL) {
				die("XOpenIM failed. Could not open input"
					" device.\n");
			}
		}
	}
	if (XSetIMValues(xw.xim, XNDestroyCallback, &destroy, NULL) != NULL)
		die("XSetIMValues failed. Could not set input method value.\n");
	xw.xic = XCreateIC(xw.xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
				XNClientWindow, xw.win, XNFocusWindow, xw.win, NULL);
	if (xw.xic == NULL)
		die("XCreateIC failed. Could not obtain input method.\n");
}

void
ximinstantiate(Display *dpy, XPointer client, XPointer call)
{
	ximopen(dpy);
	XUnregisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
					ximinstantiate, NULL);
}

void
ximdestroy(XIM xim, XPointer client, XPointer call)
{
	xw.xim = NULL;
	XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
					ximinstantiate, NULL);
}

void
xximspot(int x, int y)
{
	XPoint spot = { borderpx + x * win.cw, borderpx + (y + 1) * win.ch };
	XVaNestedList attr = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);

	XSetICValues(xw.xic, XNPreeditAttributes, attr, NULL);
	XFree(attr);
}