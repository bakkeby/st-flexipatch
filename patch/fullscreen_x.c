void set_fullscreen_status(int status)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));

    ev.xclient.type         = ClientMessage;
    ev.xclient.message_type = xw.netwmstate;
    ev.xclient.display      = xw.dpy;
    ev.xclient.window       = xw.win;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = status;
    ev.xclient.data.l[1]    = xw.netwmfullscreen;

    XSendEvent(xw.dpy, DefaultRootWindow(xw.dpy), False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

void set_fullscreen(void)
{
    set_fullscreen_status(1);
}

void unset_fullscreen(void)
{
    set_fullscreen_status(0);
}

void toggle_fullscreen(const Arg* arg)
{
    set_fullscreen_status(2);
}

