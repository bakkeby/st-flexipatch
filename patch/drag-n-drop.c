const char XdndVersion = 5;

void
xdndsel(XEvent *e)
{
	char* data;
	unsigned long result;

	Atom actualType;
	int32_t actualFormat;
	unsigned long bytesAfter;
	XEvent reply = { ClientMessage };

	reply.xclient.window = xw.XdndSourceWin;
	reply.xclient.format = 32;
	reply.xclient.data.l[0] = (long) xw.win;
	reply.xclient.data.l[2] = 0;
	reply.xclient.data.l[3] = 0;

	XGetWindowProperty((Display*) xw.dpy, e->xselection.requestor,
			e->xselection.property, 0, LONG_MAX, False,
			e->xselection.target, &actualType, &actualFormat, &result,
			&bytesAfter, (unsigned char**) &data);

	if (result == 0)
		return;

	if (data) {
		xdndpastedata(data);
		XFree(data);
	}

	if (xw.XdndSourceVersion >= 2) {
		reply.xclient.message_type = xw.XdndFinished;
		reply.xclient.data.l[1] = result;
		reply.xclient.data.l[2] = xw.XdndActionCopy;

		XSendEvent((Display*) xw.dpy, xw.XdndSourceWin, False, NoEventMask,
			 	&reply);
		XFlush((Display*) xw.dpy);
	}
}

int
xdndurldecode(char *src, char *dest)
{
	char c;
	int i = 0;

	while (*src) {
		if (*src == '%' && HEX_TO_INT(src[1]) != -1 && HEX_TO_INT(src[2]) != -1) {
			/* handle %xx escape sequences in url e.g. %20 == ' ' */
			c = (char)((HEX_TO_INT(src[1]) << 4) | HEX_TO_INT(src[2]));
			src += 3;
		} else {
			c = *src++;
		}
		if (strchr(xdndescchar, c) != NULL) {
			*dest++ = '\\';
			i++;
		}
		*dest++ = c;
		i++;
	}
	*dest++ = ' ';
	*dest = '\0';
	return i + 1;
}

void
xdndpastedata(char *data)
{
	char *pastedata, *t;
	int i = 0;

	pastedata = (char *)malloc(strlen(data) * 2 + 1);
	*pastedata = '\0';

	t = strtok(data, "\n\r");
	while(t != NULL) {
		/* Remove 'file://' prefix if it exists */
		if (strncmp(data, "file://", 7) == 0) {
			t += 7;
		}
		i += xdndurldecode(t, pastedata + i);
		t = strtok(NULL, "\n\r");
	}

	xsetsel(pastedata);
	selpaste(0);
}

void
xdndenter(XEvent *e)
{
	unsigned long count;
	Atom* formats;
	Atom real_formats[6];
	Bool list;
	Atom actualType;
	int32_t actualFormat;
	unsigned long bytesAfter;
	unsigned long i;

	list = e->xclient.data.l[1] & 1;

	if (list) {
		XGetWindowProperty((Display*) xw.dpy,
			xw.XdndSourceWin,
			xw.XdndTypeList,
			0,
			LONG_MAX,
			False,
			4,
			&actualType,
			&actualFormat,
			&count,
			&bytesAfter,
			(unsigned char**) &formats);
	} else {
		count = 0;

		if (e->xclient.data.l[2] != None)
			real_formats[count++] = e->xclient.data.l[2];
		if (e->xclient.data.l[3] != None)
			real_formats[count++] = e->xclient.data.l[3];
		if (e->xclient.data.l[4] != None)
			real_formats[count++] = e->xclient.data.l[4];

		formats = real_formats;
	}

	for (i = 0; i < count; i++) {
		if (formats[i] == xw.XtextUriList || formats[i] == xw.XtextPlain) {
			xw.XdndSourceFormat = formats[i];
			break;
		}
	}

	if (list)
		XFree(formats);
}

void
xdndpos(XEvent *e)
{
	const int32_t xabs = (e->xclient.data.l[2] >> 16) & 0xffff;
	const int32_t yabs = (e->xclient.data.l[2]) & 0xffff;
	Window dummy;
	int32_t xpos, ypos;
	XEvent reply = { ClientMessage };

	reply.xclient.window = xw.XdndSourceWin;
	reply.xclient.format = 32;
	reply.xclient.data.l[0] = (long) xw.win;
	reply.xclient.data.l[2] = 0;
	reply.xclient.data.l[3] = 0;

	XTranslateCoordinates((Display*) xw.dpy,
		XDefaultRootWindow((Display*) xw.dpy),
		(Window) xw.win,
		xabs, yabs,
		&xpos, &ypos,
		&dummy);

	reply.xclient.message_type = xw.XdndStatus;

	if (xw.XdndSourceFormat) {
		reply.xclient.data.l[1] = 1;
		if (xw.XdndSourceVersion >= 2)
			reply.xclient.data.l[4] = xw.XdndActionCopy;
	}

	XSendEvent((Display*) xw.dpy, xw.XdndSourceWin, False, NoEventMask,
			&reply);
	XFlush((Display*) xw.dpy);
}

void
xdnddrop(XEvent *e)
{
	Time time = CurrentTime;
	XEvent reply = { ClientMessage };

	reply.xclient.window = xw.XdndSourceWin;
	reply.xclient.format = 32;
	reply.xclient.data.l[0] = (long) xw.win;
	reply.xclient.data.l[2] = 0;
	reply.xclient.data.l[3] = 0;

	if (xw.XdndSourceFormat) {
		if (xw.XdndSourceVersion >= 1)
			time = e->xclient.data.l[2];

		XConvertSelection((Display*) xw.dpy, xw.XdndSelection,
				xw.XdndSourceFormat, xw.XdndSelection, (Window) xw.win, time);
	} else if (xw.XdndSourceVersion >= 2) {
		reply.xclient.message_type = xw.XdndFinished;

		XSendEvent((Display*) xw.dpy, xw.XdndSourceWin,
				False, NoEventMask, &reply);
		XFlush((Display*) xw.dpy);
	}
}
