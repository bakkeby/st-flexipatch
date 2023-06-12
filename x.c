/* See LICENSE for license details. */
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>

char *argv0;
#include "arg.h"
#include "st.h"
#include "win.h"
#if LIGATURES_PATCH
#include "hb.h"
#endif // LIGATURES_PATCH

#if THEMED_CURSOR_PATCH
#include <X11/Xcursor/Xcursor.h>
#endif // THEMED_CURSOR_PATCH

#if UNDERCURL_PATCH
/* Undercurl slope types */
enum undercurl_slope_type {
	UNDERCURL_SLOPE_ASCENDING = 0,
	UNDERCURL_SLOPE_TOP_CAP = 1,
	UNDERCURL_SLOPE_DESCENDING = 2,
	UNDERCURL_SLOPE_BOTTOM_CAP = 3
};
#endif // UNDERCURL_PATCH

/* X modifiers */
#define XK_ANY_MOD    UINT_MAX
#define XK_NO_MOD     0
#define XK_SWITCH_MOD (1<<13|1<<14)

/* function definitions used in config.h */
static void clipcopy(const Arg *);
static void clippaste(const Arg *);
static void numlock(const Arg *);
static void selpaste(const Arg *);
static void ttysend(const Arg *);
static void zoom(const Arg *);
static void zoomabs(const Arg *);
static void zoomreset(const Arg *);

#include "patch/st_include.h"
#include "patch/x_include.h"

/* config.h for applying patches and the configuration. */
#include "config.h"

#if CSI_22_23_PATCH
/* size of title stack */
#define TITLESTACKSIZE 8
#endif // CSI_22_23_PATCH

/* XEMBED messages */
#define XEMBED_FOCUS_IN  4
#define XEMBED_FOCUS_OUT 5

/* macros */
#define IS_SET(flag)		((win.mode & (flag)) != 0)
#define TRUERED(x)		(((x) & 0xff0000) >> 8)
#define TRUEGREEN(x)		(((x) & 0xff00))
#define TRUEBLUE(x)		(((x) & 0xff) << 8)

static inline ushort sixd_to_16bit(int);
static int xmakeglyphfontspecs(XftGlyphFontSpec *, const Glyph *, int, int, int);
#if WIDE_GLYPHS_PATCH
static void xdrawglyphfontspecs(const XftGlyphFontSpec *, Glyph, int, int, int, int);
#else
static void xdrawglyphfontspecs(const XftGlyphFontSpec *, Glyph, int, int, int);
#endif // WIDE_GLYPHS_PATCH
#if LIGATURES_PATCH
static void xresetfontsettings(ushort mode, Font **font, int *frcflags);
#endif // LIGATURES_PATCH
static void xdrawglyph(Glyph, int, int);
static void xclear(int, int, int, int);
static int xgeommasktogravity(int);
static int ximopen(Display *);
static void ximinstantiate(Display *, XPointer, XPointer);
static void ximdestroy(XIM, XPointer, XPointer);
static int xicdestroy(XIC, XPointer, XPointer);
static void xinit(int, int);
static void cresize(int, int);
static void xresize(int, int);
static void xhints(void);
static int xloadcolor(int, const char *, Color *);
static int xloadfont(Font *, FcPattern *);
static void xloadfonts(const char *, double);
static void xunloadfont(Font *);
static void xunloadfonts(void);
static void xsetenv(void);
static void xseturgency(int);
static int evcol(XEvent *);
static int evrow(XEvent *);

static void expose(XEvent *);
static void visibility(XEvent *);
static void unmap(XEvent *);
static void kpress(XEvent *);
static void cmessage(XEvent *);
static void resize(XEvent *);
static void focus(XEvent *);
static uint buttonmask(uint);
static void brelease(XEvent *);
static void bpress(XEvent *);
static void bmotion(XEvent *);
static void propnotify(XEvent *);
static void selnotify(XEvent *);
static void selclear_(XEvent *);
static void selrequest(XEvent *);
static void setsel(char *, Time);
#if XRESOURCES_PATCH && XRESOURCES_RELOAD_PATCH || BACKGROUND_IMAGE_PATCH && BACKGROUND_IMAGE_RELOAD_PATCH
static void sigusr1_reload(int sig);
#endif // XRESOURCES_RELOAD_PATCH | BACKGROUND_IMAGE_RELOAD_PATCH
static int mouseaction(XEvent *, uint);
static void mousesel(XEvent *, int);
static void mousereport(XEvent *);
static char *kmap(KeySym, uint);
static int match(uint, uint);

static void run(void);
static void usage(void);

static void (*handler[LASTEvent])(XEvent *) = {
	[KeyPress] = kpress,
	[ClientMessage] = cmessage,
	[ConfigureNotify] = resize,
	[VisibilityNotify] = visibility,
	[UnmapNotify] = unmap,
	[Expose] = expose,
	[FocusIn] = focus,
	[FocusOut] = focus,
	[MotionNotify] = bmotion,
	[ButtonPress] = bpress,
	[ButtonRelease] = brelease,
/*
 * Uncomment if you want the selection to disappear when you select something
 * different in another window.
 */
/*	[SelectionClear] = selclear_, */
	[SelectionNotify] = selnotify,
/*
 * PropertyNotify is only turned on when there is some INCR transfer happening
 * for the selection retrieval.
 */
	[PropertyNotify] = propnotify,
	[SelectionRequest] = selrequest,
	#if ST_EMBEDDER_PATCH
	[CreateNotify] = createnotify,
	[DestroyNotify] = destroynotify,
	#endif // ST_EMBEDDER_PATCH
};

/* Globals */
Term term;
DC dc;
XWindow xw;
XSelection xsel;
TermWindow win;

#if CSI_22_23_PATCH
static int tstki; /* title stack index */
static char *titlestack[TITLESTACKSIZE]; /* title stack */
#endif // CSI_22_23_PATCH

/* Font Ring Cache */
enum {
	FRC_NORMAL,
	FRC_ITALIC,
	FRC_BOLD,
	FRC_ITALICBOLD
};

typedef struct {
	XftFont *font;
	int flags;
	Rune unicodep;
} Fontcache;

/* Fontcache is an array now. A new font will be appended to the array. */
static Fontcache *frc = NULL;
static int frclen = 0;
static int frccap = 0;
static char *usedfont = NULL;
static double usedfontsize = 0;
static double defaultfontsize = 0;

#if ALPHA_PATCH
static char *opt_alpha = NULL;
#endif // ALPHA_PATCH
static char *opt_class = NULL;
static char **opt_cmd  = NULL;
static char *opt_embed = NULL;
static char *opt_font  = NULL;
static char *opt_io    = NULL;
static char *opt_line  = NULL;
static char *opt_name  = NULL;
static char *opt_title = NULL;
#if WORKINGDIR_PATCH
static char *opt_dir   = NULL;
#endif // WORKINGDIR_PATCH

#if ALPHA_PATCH && ALPHA_FOCUS_HIGHLIGHT_PATCH
static int focused = 0;
#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH

static uint buttons; /* bit field of pressed buttons */
#if BLINKING_CURSOR_PATCH
static int cursorblinks = 0;
#endif // BLINKING_CURSOR_PATCH
#if VISUALBELL_1_PATCH
static int bellon = 0;    /* visual bell status */
#endif // VISUALBELL_1_PATCH
#if RELATIVEBORDER_PATCH
int borderpx;
#endif // RELATIVEBORDER_PATCH
#if SWAPMOUSE_PATCH
static Cursor cursor;
static XColor xmousefg, xmousebg;
#endif // SWAPMOUSE_PATCH

#include "patch/x_include.c"

void
clipcopy(const Arg *dummy)
{
	Atom clipboard;

	free(xsel.clipboard);
	xsel.clipboard = NULL;

	if (xsel.primary != NULL) {
		xsel.clipboard = xstrdup(xsel.primary);
		clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
		XSetSelectionOwner(xw.dpy, clipboard, xw.win, CurrentTime);
	}
}

void
clippaste(const Arg *dummy)
{
	Atom clipboard;

	clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
	XConvertSelection(xw.dpy, clipboard, xsel.xtarget, clipboard,
			xw.win, CurrentTime);
}

void
numlock(const Arg *dummy)
{
	win.mode ^= MODE_NUMLOCK;
}

void
selpaste(const Arg *dummy)
{
	XConvertSelection(xw.dpy, XA_PRIMARY, xsel.xtarget, XA_PRIMARY,
			xw.win, CurrentTime);
}

void
ttysend(const Arg *arg)
{
	ttywrite(arg->s, strlen(arg->s), 1);
}

void
zoom(const Arg *arg)
{
	Arg larg;

	larg.f = usedfontsize + arg->f;
	zoomabs(&larg);
}

void
zoomabs(const Arg *arg)
{
	xunloadfonts();
	xloadfonts(usedfont, arg->f);
	#if FONT2_PATCH
	xloadsparefonts();
	#endif // FONT2_PATCH
	cresize(0, 0);
	redraw();
	xhints();
}

void
zoomreset(const Arg *arg)
{
	Arg larg;

	if (defaultfontsize > 0) {
		larg.f = defaultfontsize;
		zoomabs(&larg);
	}
}

int
evcol(XEvent *e)
{
	#if ANYSIZE_PATCH
	int x = e->xbutton.x - win.hborderpx;
	#else
	int x = e->xbutton.x - borderpx;
	#endif // ANYSIZE_PATCH
	LIMIT(x, 0, win.tw - 1);
	return x / win.cw;
}

int
evrow(XEvent *e)
{
	#if ANYSIZE_PATCH
	int y = e->xbutton.y - win.vborderpx;
	#else
	int y = e->xbutton.y - borderpx;
	#endif // ANYSIZE_PATCH
	LIMIT(y, 0, win.th - 1);
	return y / win.ch;
}

uint
buttonmask(uint button)
{
	return button == Button1 ? Button1Mask
		: button == Button2 ? Button2Mask
		: button == Button3 ? Button3Mask
		: button == Button4 ? Button4Mask
		: button == Button5 ? Button5Mask
		: 0;
}

int
mouseaction(XEvent *e, uint release)
{
	MouseShortcut *ms;
	int screen = tisaltscr() ? S_ALT : S_PRI;

	/* ignore Button<N>mask for Button<N> - it's set on release */
	uint state = e->xbutton.state & ~buttonmask(e->xbutton.button);

	for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
		if (ms->release == release &&
				ms->button == e->xbutton.button &&
				(!ms->screen || (ms->screen == screen)) &&
				(match(ms->mod, state) ||  /* exact or forced */
				 match(ms->mod, state & ~forcemousemod))) {
			ms->func(&(ms->arg));
			return 1;
		}
	}

	return 0;
}

void
mousesel(XEvent *e, int done)
{
	int type, seltype = SEL_REGULAR;
	uint state = e->xbutton.state & ~(Button1Mask | forcemousemod);

	for (type = 1; type < LEN(selmasks); ++type) {
		if (match(selmasks[type], state)) {
			seltype = type;
			break;
		}
	}
	selextend(evcol(e), evrow(e), seltype, done);
	if (done)
		setsel(getsel(), e->xbutton.time);
}

void
mousereport(XEvent *e)
{
	int len, btn, code;
	int x = evcol(e), y = evrow(e);
	int state = e->xbutton.state;
	char buf[40];
	static int ox, oy;

	if (e->type == MotionNotify) {
		if (x == ox && y == oy)
			return;
		if (!IS_SET(MODE_MOUSEMOTION) && !IS_SET(MODE_MOUSEMANY))
			return;
		/* MODE_MOUSEMOTION: no reporting if no button is pressed */
		if (IS_SET(MODE_MOUSEMOTION) && buttons == 0)
			return;

		/* Set btn to lowest-numbered pressed button, or 12 if no
		 * buttons are pressed. */
		for (btn = 1; btn <= 11 && !(buttons & (1<<(btn-1))); btn++)
			;
		code = 32;
	} else {
		btn = e->xbutton.button;
		/* Only buttons 1 through 11 can be encoded */
		if (btn < 1 || btn > 11)
			return;
		if (e->type == ButtonRelease) {
			/* MODE_MOUSEX10: no button release reporting */
			if (IS_SET(MODE_MOUSEX10))
				return;
			/* Don't send release events for the scroll wheel */
			if (btn == 4 || btn == 5)
				return;
		}
		code = 0;
	}

	ox = x;
	oy = y;

	/* Encode btn into code. If no button is pressed for a motion event in
	 * MODE_MOUSEMANY, then encode it as a release. */
	if ((!IS_SET(MODE_MOUSESGR) && e->type == ButtonRelease) || btn == 12)
		code += 3;
	else if (btn >= 8)
		code += 128 + btn - 8;
	else if (btn >= 4)
		code += 64 + btn - 4;
	else
		code += btn - 1;

	if (!IS_SET(MODE_MOUSEX10)) {
		code += ((state & ShiftMask  ) ?  4 : 0)
		      + ((state & Mod1Mask   ) ?  8 : 0) /* meta key: alt */
		      + ((state & ControlMask) ? 16 : 0);
	}

	if (IS_SET(MODE_MOUSESGR)) {
		len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c",
				code, x+1, y+1,
				e->type == ButtonRelease ? 'm' : 'M');
	} else if (x < 223 && y < 223) {
		len = snprintf(buf, sizeof(buf), "\033[M%c%c%c",
				32+code, 32+x+1, 32+y+1);
	} else {
		return;
	}

	ttywrite(buf, len, 0);
}

void
bpress(XEvent *e)
{
	int btn = e->xbutton.button;
	struct timespec now;
	#if !VIM_BROWSE_PATCH
	int snap;
	#endif // VIM_BROWSE_PATCH

	if (1 <= btn && btn <= 11)
		buttons |= 1 << (btn-1);

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	if (mouseaction(e, 0))
		return;

	if (btn == Button1) {
		/*
		 * If the user clicks below predefined timeouts specific
		 * snapping behaviour is exposed.
		 */
		clock_gettime(CLOCK_MONOTONIC, &now);
		#if VIM_BROWSE_PATCH
		int const tripleClick = TIMEDIFF(now, xsel.tclick2) <= tripleclicktimeout,
		doubleClick = TIMEDIFF(now, xsel.tclick1) <= doubleclicktimeout;
		if ((mouseYank || mouseSelect) && (tripleClick || doubleClick)) {
			if (!IS_SET(MODE_NORMAL)) normalMode();
			historyOpToggle(1, 1);
			tmoveto(evcol(e), evrow(e));
			if (tripleClick) {
				if (mouseYank) pressKeys("dVy", 3);
				if (mouseSelect) pressKeys("dV", 2);
			} else if (doubleClick) {
				if (mouseYank) pressKeys("dyiW", 4);
				if (mouseSelect) {
					tmoveto(evcol(e), evrow(e));
					pressKeys("viW", 3);
				}
			}
			historyOpToggle(-1, 1);
		} else {
			if (!IS_SET(MODE_NORMAL)) selstart(evcol(e), evrow(e), 0);
			else {
				historyOpToggle(1, 1);
				tmoveto(evcol(e), evrow(e));
				pressKeys("v", 1);
				historyOpToggle(-1, 1);
			}
		}
		#else
		if (TIMEDIFF(now, xsel.tclick2) <= tripleclicktimeout) {
			snap = SNAP_LINE;
		} else if (TIMEDIFF(now, xsel.tclick1) <= doubleclicktimeout) {
			snap = SNAP_WORD;
		} else {
			snap = 0;
		}
		#endif // VIM_BROWSE_PATCH
		xsel.tclick2 = xsel.tclick1;
		xsel.tclick1 = now;

		#if !VIM_BROWSE_PATCH
		selstart(evcol(e), evrow(e), snap);
		#endif // VIM_BROWSE_PATCH

		#if OPENURLONCLICK_PATCH
		clearurl();
		url_click = 1;
		#endif // OPENURLONCLICK_PATCH
	}
}

void
propnotify(XEvent *e)
{
	XPropertyEvent *xpev;
	Atom clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);

	xpev = &e->xproperty;
	if (xpev->state == PropertyNewValue &&
			(xpev->atom == XA_PRIMARY ||
			 xpev->atom == clipboard)) {
		selnotify(e);
	}

	#if BACKGROUND_IMAGE_PATCH
	if (pseudotransparency &&
		!strncmp(XGetAtomName(xw.dpy, e->xproperty.atom), "_NET_WM_STATE", 13)) {
		updatexy();
		redraw();
	}
	#endif // BACKGROUND_IMAGE_PATCH
}

void
selnotify(XEvent *e)
{
	ulong nitems, ofs, rem;
	int format;
	uchar *data, *last, *repl;
	Atom type, incratom, property = None;

	incratom = XInternAtom(xw.dpy, "INCR", 0);

	ofs = 0;
	if (e->type == SelectionNotify)
		property = e->xselection.property;
	else if (e->type == PropertyNotify)
		property = e->xproperty.atom;

	if (property == None)
		return;

	do {
		if (XGetWindowProperty(xw.dpy, xw.win, property, ofs,
					BUFSIZ/4, False, AnyPropertyType,
					&type, &format, &nitems, &rem,
					&data)) {
			fprintf(stderr, "Clipboard allocation failed\n");
			return;
		}

		#if BACKGROUND_IMAGE_PATCH
		if (e->type == PropertyNotify && nitems == 0 && rem == 0 && !pseudotransparency)
		#else
		if (e->type == PropertyNotify && nitems == 0 && rem == 0)
		#endif // BACKGROUND_IMAGE_PATCH
		{
			/*
			 * If there is some PropertyNotify with no data, then
			 * this is the signal of the selection owner that all
			 * data has been transferred. We won't need to receive
			 * PropertyNotify events anymore.
			 */
			MODBIT(xw.attrs.event_mask, 0, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
					&xw.attrs);
		}

		if (type == incratom) {
			/*
			 * Activate the PropertyNotify events so we receive
			 * when the selection owner does send us the next
			 * chunk of data.
			 */
			#if BACKGROUND_IMAGE_PATCH
			if (!pseudotransparency) {
			#endif // BACKGROUND_IMAGE_PATCH
			MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
					&xw.attrs);
			#if BACKGROUND_IMAGE_PATCH
			}
			#endif // BACKGROUND_IMAGE_PATCH

			/*
			 * Deleting the property is the transfer start signal.
			 */
			XDeleteProperty(xw.dpy, xw.win, (int)property);
			continue;
		}

		/*
		 * As seen in getsel:
		 * Line endings are inconsistent in the terminal and GUI world
		 * copy and pasting. When receiving some selection data,
		 * replace all '\n' with '\r'.
		 * FIXME: Fix the computer world.
		 */
		repl = data;
		last = data + nitems * format / 8;
		while ((repl = memchr(repl, '\n', last - repl))) {
			*repl++ = '\r';
		}

		if (IS_SET(MODE_BRCKTPASTE) && ofs == 0)
			ttywrite("\033[200~", 6, 0);
		ttywrite((char *)data, nitems * format / 8, 1);
		if (IS_SET(MODE_BRCKTPASTE) && rem == 0)
			ttywrite("\033[201~", 6, 0);
		XFree(data);
		/* number of 32-bit chunks returned */
		ofs += nitems * format / 32;
	} while (rem > 0);

	/*
	 * Deleting the property again tells the selection owner to send the
	 * next data chunk in the property.
	 */
	XDeleteProperty(xw.dpy, xw.win, (int)property);
}

void
xclipcopy(void)
{
	clipcopy(NULL);
}

void
selclear_(XEvent *e)
{
	selclear();
}

void
selrequest(XEvent *e)
{
	XSelectionRequestEvent *xsre;
	XSelectionEvent xev;
	Atom xa_targets, string, clipboard;
	char *seltext;

	xsre = (XSelectionRequestEvent *) e;
	xev.type = SelectionNotify;
	xev.requestor = xsre->requestor;
	xev.selection = xsre->selection;
	xev.target = xsre->target;
	xev.time = xsre->time;
	if (xsre->property == None)
		xsre->property = xsre->target;

	/* reject */
	xev.property = None;

	xa_targets = XInternAtom(xw.dpy, "TARGETS", 0);
	if (xsre->target == xa_targets) {
		/* respond with the supported type */
		string = xsel.xtarget;
		XChangeProperty(xsre->display, xsre->requestor, xsre->property,
				XA_ATOM, 32, PropModeReplace,
				(uchar *) &string, 1);
		xev.property = xsre->property;
	} else if (xsre->target == xsel.xtarget || xsre->target == XA_STRING) {
		/*
		 * xith XA_STRING non ascii characters may be incorrect in the
		 * requestor. It is not our problem, use utf8.
		 */
		clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
		if (xsre->selection == XA_PRIMARY) {
			seltext = xsel.primary;
		} else if (xsre->selection == clipboard) {
			seltext = xsel.clipboard;
		} else {
			fprintf(stderr,
				"Unhandled clipboard selection 0x%lx\n",
				xsre->selection);
			return;
		}
		if (seltext != NULL) {
			XChangeProperty(xsre->display, xsre->requestor,
					xsre->property, xsre->target,
					8, PropModeReplace,
					(uchar *)seltext, strlen(seltext));
			xev.property = xsre->property;
		}
	}

	/* all done, send a notification to the listener */
	if (!XSendEvent(xsre->display, xsre->requestor, 1, 0, (XEvent *) &xev))
		fprintf(stderr, "Error sending SelectionNotify event\n");
}

void
setsel(char *str, Time t)
{
	if (!str)
		return;

	free(xsel.primary);
	xsel.primary = str;

	XSetSelectionOwner(xw.dpy, XA_PRIMARY, xw.win, t);
	if (XGetSelectionOwner(xw.dpy, XA_PRIMARY) != xw.win)
		selclear();

	#if CLIPBOARD_PATCH
	clipcopy(NULL);
	#endif // CLIPBOARD_PATCH
}

#if XRESOURCES_PATCH && XRESOURCES_RELOAD_PATCH || BACKGROUND_IMAGE_PATCH && BACKGROUND_IMAGE_RELOAD_PATCH
void
sigusr1_reload(int sig)
{
	#if XRESOURCES_PATCH && XRESOURCES_RELOAD_PATCH
	reload_config(sig);
	#endif // XRESOURCES_RELOAD_PATCH
	#if BACKGROUND_IMAGE_PATCH && BACKGROUND_IMAGE_RELOAD_PATCH
	reload_image();
	#endif // BACKGROUND_IMAGE_RELOAD_PATCH
	signal(SIGUSR1, sigusr1_reload);
}
#endif // XRESOURCES_RELOAD_PATCH | BACKGROUND_IMAGE_RELOAD_PATCH

void
xsetsel(char *str)
{
	setsel(str, CurrentTime);
}

void
brelease(XEvent *e)
{
	int btn = e->xbutton.button;

	if (1 <= btn && btn <= 11)
		buttons &= ~(1 << (btn-1));

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	if (mouseaction(e, 1))
		return;
	#if VIM_BROWSE_PATCH
	if (btn == Button1 && !IS_SET(MODE_NORMAL)) {
		mousesel(e, 1);
		#if OPENURLONCLICK_PATCH
		if (url_click && e->xkey.state & url_opener_modkey)
			openUrlOnClick(evcol(e), evrow(e), url_opener);
		#endif // OPENURLONCLICK_PATCH
	}
	#else
	if (btn == Button1) {
		mousesel(e, 1);
		#if OPENURLONCLICK_PATCH
		if (url_click && e->xkey.state & url_opener_modkey)
			openUrlOnClick(evcol(e), evrow(e), url_opener);
		#endif // OPENURLONCLICK_PATCH
	}
	#endif // VIM_BROWSE_PATCH
	#if RIGHTCLICKTOPLUMB_PATCH
	else if (btn == Button3)
		plumb(xsel.primary);
	#endif // RIGHTCLICKTOPLUMB_PATCH
}

void
bmotion(XEvent *e)
{
	#if HIDECURSOR_PATCH
	if (!xw.pointerisvisible) {
		#if SWAPMOUSE_PATCH
		if (win.mode & MODE_MOUSE)
			XUndefineCursor(xw.dpy, xw.win);
		else
			XDefineCursor(xw.dpy, xw.win, xw.vpointer);
		#else
		XDefineCursor(xw.dpy, xw.win, xw.vpointer);
		#endif // SWAPMOUSE_PATCH
		xw.pointerisvisible = 1;
		if (!IS_SET(MODE_MOUSEMANY))
			xsetpointermotion(0);
	}
	#endif // HIDECURSOR_PATCH
	#if OPENURLONCLICK_PATCH
	#if VIM_BROWSE_PATCH
	if (!IS_SET(MODE_NORMAL))
	#endif // VIM_BROWSE_PATCH
	if (!IS_SET(MODE_MOUSE)) {
		if (!(e->xbutton.state & Button1Mask) && detecturl(evcol(e), evrow(e), 1))
			XDefineCursor(xw.dpy, xw.win, xw.upointer);
		else
			XDefineCursor(xw.dpy, xw.win, xw.vpointer);
	}
	url_click = 0;
	#endif // OPENURLONCLICK_PATCH

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	mousesel(e, 0);
}

void
cresize(int width, int height)
{
	int col, row;

	if (width != 0)
		win.w = width;
	if (height != 0)
		win.h = height;

	col = (win.w - 2 * borderpx) / win.cw;
	row = (win.h - 2 * borderpx) / win.ch;
	col = MAX(1, col);
	row = MAX(1, row);

	#if ANYSIZE_PATCH
	win.hborderpx = (win.w - col * win.cw) / 2;
	win.vborderpx = (win.h - row * win.ch) / 2;
	#endif // ANYSIZE_PATCH

	tresize(col, row);
	xresize(col, row);
	ttyresize(win.tw, win.th);
}

void
xresize(int col, int row)
{
	win.tw = col * win.cw;
	win.th = row * win.ch;

	#if !SINGLE_DRAWABLE_BUFFER_PATCH
	XFreePixmap(xw.dpy, xw.buf);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			#if ALPHA_PATCH
			xw.depth
			#else
			DefaultDepth(xw.dpy, xw.scr)
			#endif // ALPHA_PATCH
	);
	XftDrawChange(xw.draw, xw.buf);
	#endif // SINGLE_DRAWABLE_BUFFER_PATCH
	xclear(0, 0, win.w, win.h);

	/* resize to new width */
	xw.specbuf = xrealloc(xw.specbuf, col * sizeof(GlyphFontSpec));
}

ushort
sixd_to_16bit(int x)
{
	return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

int
xloadcolor(int i, const char *name, Color *ncolor)
{
	XRenderColor color = { .alpha = 0xffff };

	if (!name) {
		if (BETWEEN(i, 16, 255)) { /* 256 color */
			if (i < 6*6*6+16) { /* same colors as xterm */
				color.red   = sixd_to_16bit( ((i-16)/36)%6 );
				color.green = sixd_to_16bit( ((i-16)/6) %6 );
				color.blue  = sixd_to_16bit( ((i-16)/1) %6 );
			} else { /* greyscale */
				color.red = 0x0808 + 0x0a0a * (i - (6*6*6+16));
				color.green = color.blue = color.red;
			}
			return XftColorAllocValue(xw.dpy, xw.vis,
			                          xw.cmap, &color, ncolor);
		} else
			name = colorname[i];
	}

	return XftColorAllocName(xw.dpy, xw.vis, xw.cmap, name, ncolor);
}

#if VIM_BROWSE_PATCH
void normalMode()
{
	#if OPENURLONCLICK_PATCH
	clearurl();
	restoremousecursor();
	#endif // OPENURLONCLICK_PATCH
	historyModeToggle((win.mode ^=MODE_NORMAL) & MODE_NORMAL);
}
#endif // VIM_BROWSE_PATCH

#if ALPHA_PATCH && ALPHA_FOCUS_HIGHLIGHT_PATCH
void
xloadalpha(void)
{
	float const usedAlpha = focused ? alpha : alphaUnfocused;
	if (opt_alpha) alpha = strtof(opt_alpha, NULL);
	dc.col[defaultbg].color.alpha = (unsigned short)(0xffff * usedAlpha);
	dc.col[defaultbg].pixel &= 0x00FFFFFF;
	dc.col[defaultbg].pixel |= (unsigned char)(0xff * usedAlpha) << 24;
}
#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH

#if ALPHA_PATCH && ALPHA_FOCUS_HIGHLIGHT_PATCH
void
xloadcols(void)
{
	static int loaded;
	Color *cp;

	if (!loaded) {
		dc.collen = 1 + (defaultbg = MAX(LEN(colorname), 256));
		dc.col = xmalloc((dc.collen) * sizeof(Color));
	}

	for (int i = 0; i+1 < dc.collen; ++i)
		if (!xloadcolor(i, NULL, &dc.col[i])) {
			if (colorname[i])
				die("could not allocate color '%s'\n", colorname[i]);
			else
				die("could not allocate color %d\n", i);
		}
	if (dc.collen) // cannot die, as the color is already loaded.
		xloadcolor(focused ? bg : bgUnfocused, NULL, &dc.col[defaultbg]);

	xloadalpha();
	loaded = 1;
}
#else
void
xloadcols(void)
{
	int i;
	static int loaded;
	Color *cp;

	if (loaded) {
		for (cp = dc.col; cp < &dc.col[dc.collen]; ++cp)
			XftColorFree(xw.dpy, xw.vis, xw.cmap, cp);
	} else {
		dc.collen = MAX(LEN(colorname), 256);
		dc.col = xmalloc(dc.collen * sizeof(Color));
	}

	for (i = 0; i < dc.collen; i++)
		if (!xloadcolor(i, NULL, &dc.col[i])) {
			if (colorname[i])
				die("could not allocate color '%s'\n", colorname[i]);
			else
				die("could not allocate color %d\n", i);
		}
	#if ALPHA_PATCH
	/* set alpha value of bg color */
	if (opt_alpha)
		alpha = strtof(opt_alpha, NULL);
	dc.col[defaultbg].color.alpha = (unsigned short)(0xffff * alpha);
	dc.col[defaultbg].pixel &= 0x00FFFFFF;
	dc.col[defaultbg].pixel |= (unsigned char)(0xff * alpha) << 24;
	#endif // ALPHA_PATCH
	loaded = 1;
}
#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH

int
xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b)
{
	if (!BETWEEN(x, 0, dc.collen))
		return 1;

	*r = dc.col[x].color.red >> 8;
	*g = dc.col[x].color.green >> 8;
	*b = dc.col[x].color.blue >> 8;

	return 0;
}

int
xsetcolorname(int x, const char *name)
{
	Color ncolor;

	if (!BETWEEN(x, 0, dc.collen))
		return 1;

	if (!xloadcolor(x, name, &ncolor))
		return 1;

	XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[x]);
	dc.col[x] = ncolor;

	#if ALPHA_PATCH
	/* set alpha value of bg color */
	if (x == defaultbg) {
		if (opt_alpha)
			alpha = strtof(opt_alpha, NULL);
		dc.col[defaultbg].color.alpha = (unsigned short)(0xffff * alpha);
		dc.col[defaultbg].pixel &= 0x00FFFFFF;
		dc.col[defaultbg].pixel |= (unsigned char)(0xff * alpha) << 24;
	}
	#endif // ALPHA_PATCH
	return 0;
}

/*
 * Absolute coordinates.
 */
void
xclear(int x1, int y1, int x2, int y2)
{
	#if BACKGROUND_IMAGE_PATCH
	if (pseudotransparency)
		XSetTSOrigin(xw.dpy, xw.bggc, -win.x, -win.y);
	XFillRectangle(xw.dpy, xw.buf, xw.bggc, x1, y1, x2-x1, y2-y1);
	#elif INVERT_PATCH
	Color c;
	c = dc.col[IS_SET(MODE_REVERSE)? defaultfg : defaultbg];
	if (invertcolors) {
		c = invertedcolor(&c);
	}
	XftDrawRect(xw.draw, &c, x1, y1, x2-x1, y2-y1);
	#else
	XftDrawRect(xw.draw,
			&dc.col[IS_SET(MODE_REVERSE)? defaultfg : defaultbg],
			x1, y1, x2-x1, y2-y1);
	#endif // INVERT_PATCH
}

void
xclearwin(void)
{
	xclear(0, 0, win.w, win.h);
}

void
xhints(void)
{
	#if XRESOURCES_PATCH
	XClassHint class = {opt_name ? opt_name : "st",
	                    opt_class ? opt_class : "St"};
	#else
	XClassHint class = {opt_name ? opt_name : termname,
	                    opt_class ? opt_class : termname};
	#endif // XRESOURCES_PATCH
	XWMHints wm = {.flags = InputHint, .input = 1};
	XSizeHints *sizeh;

	sizeh = XAllocSizeHints();

	sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
	sizeh->height = win.h;
	sizeh->width = win.w;
	#if ANYSIZE_PATCH || ANYSIZE_SIMPLE_PATCH
	sizeh->height_inc = 1;
	sizeh->width_inc = 1;
	#else
	sizeh->height_inc = win.ch;
	sizeh->width_inc = win.cw;
	#endif // ANYSIZE_PATCH
	sizeh->base_height = 2 * borderpx;
	sizeh->base_width = 2 * borderpx;
	sizeh->min_height = win.ch + 2 * borderpx;
	sizeh->min_width = win.cw + 2 * borderpx;
	if (xw.isfixed) {
		sizeh->flags |= PMaxSize;
		sizeh->min_width = sizeh->max_width = win.w;
		sizeh->min_height = sizeh->max_height = win.h;
	}
	if (xw.gm & (XValue|YValue)) {
		sizeh->flags |= USPosition | PWinGravity;
		sizeh->x = xw.l;
		sizeh->y = xw.t;
		sizeh->win_gravity = xgeommasktogravity(xw.gm);
	}

	XSetWMProperties(xw.dpy, xw.win, NULL, NULL, NULL, 0, sizeh, &wm,
			&class);
	XFree(sizeh);
}

int
xgeommasktogravity(int mask)
{
	switch (mask & (XNegative|YNegative)) {
	case 0:
		return NorthWestGravity;
	case XNegative:
		return NorthEastGravity;
	case YNegative:
		return SouthWestGravity;
	}

	return SouthEastGravity;
}

int
ximopen(Display *dpy)
{
	XIMCallback imdestroy = { .client_data = NULL, .callback = ximdestroy };
	XICCallback icdestroy = { .client_data = NULL, .callback = xicdestroy };

	xw.ime.xim = XOpenIM(xw.dpy, NULL, NULL, NULL);
	if (xw.ime.xim == NULL)
		return 0;

	if (XSetIMValues(xw.ime.xim, XNDestroyCallback, &imdestroy, NULL))
		fprintf(stderr, "XSetIMValues: "
		                "Could not set XNDestroyCallback.\n");

	xw.ime.spotlist = XVaCreateNestedList(0, XNSpotLocation, &xw.ime.spot,
	                                      NULL);

	if (xw.ime.xic == NULL) {
		xw.ime.xic = XCreateIC(xw.ime.xim, XNInputStyle,
		                       XIMPreeditNothing | XIMStatusNothing,
		                       XNClientWindow, xw.win,
		                       XNDestroyCallback, &icdestroy,
		                       NULL);
	}
	if (xw.ime.xic == NULL)
		fprintf(stderr, "XCreateIC: Could not create input context.\n");

	return 1;
}

void
ximinstantiate(Display *dpy, XPointer client, XPointer call)
{
	if (ximopen(dpy))
		XUnregisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
		                                 ximinstantiate, NULL);
}

void
ximdestroy(XIM xim, XPointer client, XPointer call)
{
	xw.ime.xim = NULL;
	XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
	                               ximinstantiate, NULL);
	XFree(xw.ime.spotlist);
}

int
xicdestroy(XIC xim, XPointer client, XPointer call)
{
	xw.ime.xic = NULL;
	return 1;
}

int
xloadfont(Font *f, FcPattern *pattern)
{
	FcPattern *configured;
	FcPattern *match;
	FcResult result;
	XGlyphInfo extents;
	int wantattr, haveattr;

	/*
	 * Manually configure instead of calling XftMatchFont
	 * so that we can use the configured pattern for
	 * "missing glyph" lookups.
	 */
	configured = FcPatternDuplicate(pattern);
	if (!configured)
		return 1;

	FcConfigSubstitute(NULL, configured, FcMatchPattern);
	XftDefaultSubstitute(xw.dpy, xw.scr, configured);

	#if USE_XFTFONTMATCH_PATCH
	match = XftFontMatch(xw.dpy, xw.scr, pattern, &result);
	#else
	match = FcFontMatch(NULL, configured, &result);
	#endif // USE_XFTFONTMATCH_PATCH
	if (!match) {
		FcPatternDestroy(configured);
		return 1;
	}

	if (!(f->match = XftFontOpenPattern(xw.dpy, match))) {
		FcPatternDestroy(configured);
		FcPatternDestroy(match);
		return 1;
	}

	if ((XftPatternGetInteger(pattern, "slant", 0, &wantattr) ==
	    XftResultMatch)) {
		/*
		 * Check if xft was unable to find a font with the appropriate
		 * slant but gave us one anyway. Try to mitigate.
		 */
		if ((XftPatternGetInteger(f->match->pattern, "slant", 0,
		    &haveattr) != XftResultMatch) || haveattr < wantattr) {
			f->badslant = 1;
			fputs("font slant does not match\n", stderr);
		}
	}

	if ((XftPatternGetInteger(pattern, "weight", 0, &wantattr) ==
	    XftResultMatch)) {
		if ((XftPatternGetInteger(f->match->pattern, "weight", 0,
		    &haveattr) != XftResultMatch) || haveattr != wantattr) {
			f->badweight = 1;
			fputs("font weight does not match\n", stderr);
		}
	}

	XftTextExtentsUtf8(xw.dpy, f->match,
		(const FcChar8 *) ascii_printable,
		strlen(ascii_printable), &extents);

	f->set = NULL;
	f->pattern = configured;

	f->ascent = f->match->ascent;
	f->descent = f->match->descent;
	f->lbearing = 0;
	f->rbearing = f->match->max_advance_width;

	f->height = f->ascent + f->descent;
	#if WIDE_GLYPH_SPACING_PATCH
	f->width = DIVCEIL(extents.xOff > 18 ? extents.xOff / 3 : extents.xOff, strlen(ascii_printable));
	#else
	f->width = DIVCEIL(extents.xOff, strlen(ascii_printable));
	#endif // WIDE_GLYPH_SPACING_PATCH

	return 0;
}

void
xloadfonts(const char *fontstr, double fontsize)
{
	FcPattern *pattern;
	double fontval;

	if (fontstr[0] == '-')
		pattern = XftXlfdParse(fontstr, False, False);
	else
		pattern = FcNameParse((const FcChar8 *)fontstr);

	if (!pattern)
		die("can't open font %s\n", fontstr);

	if (fontsize > 1) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternDel(pattern, FC_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, (double)fontsize);
		usedfontsize = fontsize;
	} else {
		if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) ==
				FcResultMatch) {
			usedfontsize = fontval;
		} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) ==
				FcResultMatch) {
			usedfontsize = -1;
		} else {
			/*
			 * Default font size is 12, if none given. This is to
			 * have a known usedfontsize value.
			 */
			FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12);
			usedfontsize = 12;
		}
		defaultfontsize = usedfontsize;
	}

	if (xloadfont(&dc.font, pattern))
		die("can't open font %s\n", fontstr);

	if (usedfontsize < 0) {
		FcPatternGetDouble(dc.font.match->pattern,
		                   FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
		if (fontsize == 0)
			defaultfontsize = fontval;
	}

	/* Setting character width and height. */
	win.cw = ceilf(dc.font.width * cwscale);
	win.ch = ceilf(dc.font.height * chscale);
	#if VERTCENTER_PATCH
	win.cyo = ceilf(dc.font.height * (chscale - 1) / 2);
	#endif // VERTCENTER_PATCH

	#if RELATIVEBORDER_PATCH
	borderpx = (int) ceilf(((float)borderperc / 100) * win.cw);
	#endif // RELATIVEBORDER_PATCH
	FcPatternDel(pattern, FC_SLANT);
	#if !DISABLE_ITALIC_FONTS_PATCH
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	#endif // DISABLE_ITALIC_FONTS_PATCH
	if (xloadfont(&dc.ifont, pattern))
		die("can't open font %s\n", fontstr);

	FcPatternDel(pattern, FC_WEIGHT);
	#if !DISABLE_BOLD_FONTS_PATCH
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	#endif // DISABLE_BOLD_FONTS_PATCH
	if (xloadfont(&dc.ibfont, pattern))
		die("can't open font %s\n", fontstr);

	FcPatternDel(pattern, FC_SLANT);
	#if !DISABLE_ROMAN_FONTS_PATCH
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	#endif // DISABLE_ROMAN_FONTS_PATCH
	if (xloadfont(&dc.bfont, pattern))
		die("can't open font %s\n", fontstr);

	FcPatternDestroy(pattern);
}

void
xunloadfont(Font *f)
{
	XftFontClose(xw.dpy, f->match);
	FcPatternDestroy(f->pattern);
	if (f->set)
		FcFontSetDestroy(f->set);
}

void
xunloadfonts(void)
{
	#if LIGATURES_PATCH
	/* Clear Harfbuzz font cache. */
	hbunloadfonts();
	#endif // LIGATURES_PATCH

	/* Free the loaded fonts in the font cache.  */
	while (frclen > 0)
		XftFontClose(xw.dpy, frc[--frclen].font);

	xunloadfont(&dc.font);
	xunloadfont(&dc.bfont);
	xunloadfont(&dc.ifont);
	xunloadfont(&dc.ibfont);
}

void
xinit(int cols, int rows)
{
	XGCValues gcvalues;
	#if HIDECURSOR_PATCH
	Pixmap blankpm;
	#elif !SWAPMOUSE_PATCH
	Cursor cursor;
	#endif // HIDECURSOR_PATCH
	Window parent;
	pid_t thispid = getpid();
	#if !SWAPMOUSE_PATCH
	XColor xmousefg, xmousebg;
	#endif // SWAPMOUSE_PATCH
	#if ALPHA_PATCH
	XWindowAttributes attr;
	XVisualInfo vis;
	#endif // ALPHA_PATCH

	#if !XRESOURCES_PATCH
	if (!(xw.dpy = XOpenDisplay(NULL)))
		die("can't open display\n");
	#endif // XRESOURCES_PATCH
	xw.scr = XDefaultScreen(xw.dpy);

	#if ALPHA_PATCH
	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0)))) {
		parent = XRootWindow(xw.dpy, xw.scr);
		xw.depth = 32;
	} else {
		XGetWindowAttributes(xw.dpy, parent, &attr);
		xw.depth = attr.depth;
	}

	XMatchVisualInfo(xw.dpy, xw.scr, xw.depth, TrueColor, &vis);
	xw.vis = vis.visual;
	#else
	xw.vis = XDefaultVisual(xw.dpy, xw.scr);
	#endif // ALPHA_PATCH

	/* font */
	if (!FcInit())
		die("could not init fontconfig.\n");

	usedfont = (opt_font == NULL)? font : opt_font;
	xloadfonts(usedfont, 0);

	#if FONT2_PATCH
	/* spare fonts */
	xloadsparefonts();
	#endif // FONT2_PATCH

	/* colors */
	#if ALPHA_PATCH
	xw.cmap = XCreateColormap(xw.dpy, parent, xw.vis, None);
	#else
	xw.cmap = XDefaultColormap(xw.dpy, xw.scr);
	#endif // ALPHA_PATCH
	xloadcols();

	/* adjust fixed window geometry */
	#if ANYSIZE_PATCH
	win.w = 2 * win.hborderpx + cols * win.cw;
	win.h = 2 * win.vborderpx + rows * win.ch;
	#else
	win.w = 2 * borderpx + cols * win.cw;
	win.h = 2 * borderpx + rows * win.ch;
	#endif // ANYSIZE_PATCH
	if (xw.gm & XNegative)
		xw.l += DisplayWidth(xw.dpy, xw.scr) - win.w - 2;
	if (xw.gm & YNegative)
		xw.t += DisplayHeight(xw.dpy, xw.scr) - win.h - 2;

	/* Events */
	xw.attrs.background_pixel = dc.col[defaultbg].pixel;
	xw.attrs.border_pixel = dc.col[defaultbg].pixel;
	xw.attrs.bit_gravity = NorthWestGravity;
	xw.attrs.event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask
		| ExposureMask | VisibilityChangeMask | StructureNotifyMask
		| ButtonMotionMask | ButtonPressMask | ButtonReleaseMask
		#if ST_EMBEDDER_PATCH
		| SubstructureNotifyMask | SubstructureRedirectMask
		#endif // ST_EMBEDDER_PATCH
		;
	xw.attrs.colormap = xw.cmap;
	#if OPENURLONCLICK_PATCH
	xw.attrs.event_mask |= PointerMotionMask;
	#endif // OPENURLONCLICK_PATCH

	#if !ALPHA_PATCH
	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0))))
		parent = XRootWindow(xw.dpy, xw.scr);
	#endif // ALPHA_PATCH
	xw.win = XCreateWindow(xw.dpy, parent, xw.l, xw.t,
			#if ALPHA_PATCH
			win.w, win.h, 0, xw.depth, InputOutput,
			#else
			win.w, win.h, 0, XDefaultDepth(xw.dpy, xw.scr), InputOutput,
			#endif // ALPHA_PATCH
			xw.vis, CWBackPixel | CWBorderPixel | CWBitGravity
			| CWEventMask | CWColormap, &xw.attrs);

	memset(&gcvalues, 0, sizeof(gcvalues));
	gcvalues.graphics_exposures = False;

	#if ALPHA_PATCH
	#if SINGLE_DRAWABLE_BUFFER_PATCH
	xw.buf = xw.win;
	#else
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h, xw.depth);
	#endif // SINGLE_DRAWABLE_BUFFER_PATCH
	dc.gc = XCreateGC(xw.dpy, xw.buf, GCGraphicsExposures, &gcvalues);
	#else
	dc.gc = XCreateGC(xw.dpy, parent, GCGraphicsExposures,
			&gcvalues);
	#if SINGLE_DRAWABLE_BUFFER_PATCH
	xw.buf = xw.win;
	#else
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			DefaultDepth(xw.dpy, xw.scr));
	#endif // SINGLE_DRAWABLE_BUFFER_PATCH
	#endif // ALPHA_PATCH
	XSetForeground(xw.dpy, dc.gc, dc.col[defaultbg].pixel);
	XFillRectangle(xw.dpy, xw.buf, dc.gc, 0, 0, win.w, win.h);

	/* font spec buffer */
	xw.specbuf = xmalloc(cols * sizeof(GlyphFontSpec));

	/* Xft rendering context */
	xw.draw = XftDrawCreate(xw.dpy, xw.buf, xw.vis, xw.cmap);

	/* input methods */
	if (!ximopen(xw.dpy)) {
		XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
		                               ximinstantiate, NULL);
	}

	/* white cursor, black outline */
	#if HIDECURSOR_PATCH
	xw.pointerisvisible = 1;
	#if THEMED_CURSOR_PATCH
	xw.vpointer = XcursorLibraryLoadCursor(xw.dpy, mouseshape);
	#else
	xw.vpointer = XCreateFontCursor(xw.dpy, mouseshape);
	#endif // THEMED_CURSOR_PATCH
	XDefineCursor(xw.dpy, xw.win, xw.vpointer);
	#elif THEMED_CURSOR_PATCH
	cursor = XcursorLibraryLoadCursor(xw.dpy, mouseshape);
	XDefineCursor(xw.dpy, xw.win, cursor);
	#else
	cursor = XCreateFontCursor(xw.dpy, mouseshape);
	XDefineCursor(xw.dpy, xw.win, cursor);
	#endif // HIDECURSOR_PATCH

	#if !THEMED_CURSOR_PATCH
	if (XParseColor(xw.dpy, xw.cmap, colorname[mousefg], &xmousefg) == 0) {
		xmousefg.red   = 0xffff;
		xmousefg.green = 0xffff;
		xmousefg.blue  = 0xffff;
	}

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousebg], &xmousebg) == 0) {
		xmousebg.red   = 0x0000;
		xmousebg.green = 0x0000;
		xmousebg.blue  = 0x0000;
	}
	#endif // THEMED_CURSOR_PATCH

	#if HIDECURSOR_PATCH
	#if !THEMED_CURSOR_PATCH
	XRecolorCursor(xw.dpy, xw.vpointer, &xmousefg, &xmousebg);
	#endif // THEMED_CURSOR_PATCH
	blankpm = XCreateBitmapFromData(xw.dpy, xw.win, &(char){0}, 1, 1);
	xw.bpointer = XCreatePixmapCursor(xw.dpy, blankpm, blankpm,
					  &xmousefg, &xmousebg, 0, 0);
	#elif !THEMED_CURSOR_PATCH
	XRecolorCursor(xw.dpy, cursor, &xmousefg, &xmousebg);
	#endif // HIDECURSOR_PATCH

	#if OPENURLONCLICK_PATCH
	xw.upointer = XCreateFontCursor(xw.dpy, XC_hand2);
	#if !HIDECURSOR_PATCH
	xw.vpointer = cursor;
	xw.pointerisvisible = 1;
	#endif // HIDECURSOR_PATCH
	#endif // OPENURLONCLICK_PATCH

	xw.xembed = XInternAtom(xw.dpy, "_XEMBED", False);
	xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
	xw.netwmname = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
	xw.netwmiconname = XInternAtom(xw.dpy, "_NET_WM_ICON_NAME", False);
	XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);

	#if NETWMICON_PATCH
	xw.netwmicon = XInternAtom(xw.dpy, "_NET_WM_ICON", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmicon, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)&icon, LEN(icon));
	#endif //NETWMICON_PATCH

	#if NO_WINDOW_DECORATIONS_PATCH
	Atom motifwmhints = XInternAtom(xw.dpy, "_MOTIF_WM_HINTS", False);
	unsigned int data[] = { 0x2, 0x0, 0x0, 0x0, 0x0 };
	XChangeProperty(xw.dpy, xw.win, motifwmhints, motifwmhints, 16,
				PropModeReplace, (unsigned char *)data, 5);
	#endif // NO_WINDOW_DECORATIONS_PATCH

	xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmpid, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)&thispid, 1);

	#if FULLSCREEN_PATCH
	xw.netwmstate = XInternAtom(xw.dpy, "_NET_WM_STATE", False);
	xw.netwmfullscreen = XInternAtom(xw.dpy, "_NET_WM_STATE_FULLSCREEN", False);
	#endif // FULLSCREEN_PATCH

	win.mode = MODE_NUMLOCK;
	resettitle();
	xhints();
	XMapWindow(xw.dpy, xw.win);
	XSync(xw.dpy, False);

	clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
	clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
	xsel.primary = NULL;
	xsel.clipboard = NULL;
	xsel.xtarget = XInternAtom(xw.dpy, "UTF8_STRING", 0);
	if (xsel.xtarget == None)
		xsel.xtarget = XA_STRING;

	#if BOXDRAW_PATCH
	boxdraw_xinit(xw.dpy, xw.cmap, xw.draw, xw.vis);
	#endif // BOXDRAW_PATCH
}

#if LIGATURES_PATCH
void
xresetfontsettings(ushort mode, Font **font, int *frcflags)
{
	*font = &dc.font;
	if ((mode & ATTR_ITALIC) && (mode & ATTR_BOLD)) {
		*font = &dc.ibfont;
		*frcflags = FRC_ITALICBOLD;
	} else if (mode & ATTR_ITALIC) {
		*font = &dc.ifont;
		*frcflags = FRC_ITALIC;
	} else if (mode & ATTR_BOLD) {
		*font = &dc.bfont;
		*frcflags = FRC_BOLD;
	}
}
#endif // LIGATURES_PATCH

int
xmakeglyphfontspecs(XftGlyphFontSpec *specs, const Glyph *glyphs, int len, int x, int y)
{
	#if ANYSIZE_PATCH
	float winx = win.hborderpx + x * win.cw, winy = win.vborderpx + y * win.ch, xp, yp;
	#else
	float winx = borderpx + x * win.cw, winy = borderpx + y * win.ch, xp, yp;
	#endif // ANYSIZE_PATCH
	ushort mode, prevmode = USHRT_MAX;
	Font *font = &dc.font;
	int frcflags = FRC_NORMAL;
	float runewidth = win.cw;
	Rune rune;
	FT_UInt glyphidx;
	FcResult fcres;
	FcPattern *fcpattern, *fontpattern;
	FcFontSet *fcsets[] = { NULL };
	FcCharSet *fccharset;
	int i, f, numspecs = 0;
	#if LIGATURES_PATCH
	int length = 0, start = 0;
	HbTransformData shaped = { 0 };

	/* Initial values. */
	mode = prevmode = glyphs[0].mode;
	xresetfontsettings(mode, &font, &frcflags);
	#endif // LIGATURES_PATCH

	#if VERTCENTER_PATCH
	for (i = 0, xp = winx, yp = winy + font->ascent + win.cyo; i < len; ++i)
	#else
	for (i = 0, xp = winx, yp = winy + font->ascent; i < len; ++i)
	#endif // VERTCENTER_PATCH
	{
		/* Fetch rune and mode for current glyph. */
		#if VIM_BROWSE_PATCH
		Glyph g = glyphs[i];
		historyOverlay(x+i, y, &g);
		rune = g.u;
		mode = g.mode;
		#elif LIGATURES_PATCH
		mode = glyphs[i].mode;
		#else
		rune = glyphs[i].u;
		mode = glyphs[i].mode;
		#endif // VIM_BROWSE_PATCH | LIGATURES_PATCH

		/* Skip dummy wide-character spacing. */
		#if LIGATURES_PATCH
		if (mode & ATTR_WDUMMY)
			continue;

		if (
			prevmode != mode
			|| ATTRCMP(glyphs[start], glyphs[i])
			|| selected(x + i, y) != selected(x + start, y)
			|| i == (len - 1)
		) {
			/* Handle 1-character wide segments and end of line */
			length = i - start;
			if (i == start) {
				length = 1;
			} else if (i == (len - 1)) {
				length = (i - start + 1);
			}

			/* Shape the segment. */
			hbtransform(&shaped, font->match, glyphs, start, length);
			for (int code_idx = 0; code_idx < shaped.count; code_idx++) {
				rune = glyphs[start + code_idx].u;
				runewidth = win.cw * ((glyphs[start + code_idx].mode & ATTR_WIDE) ? 2.0f : 1.0f);

				if (glyphs[start + code_idx].mode & ATTR_WDUMMY)
					continue;

				#if BOXDRAW_PATCH
				if (glyphs[start + code_idx].mode & ATTR_BOXDRAW) {
					/* minor shoehorning: boxdraw uses only this ushort */
					specs[numspecs].font = font->match;
					specs[numspecs].glyph = boxdrawindex(&glyphs[start + code_idx]);
					specs[numspecs].x = xp;
					specs[numspecs].y = yp;
					xp += runewidth;
					numspecs++;
				} else
				#endif // BOXDRAW_PATCH
				if (shaped.glyphs[code_idx].codepoint != 0) {
					/* If symbol is found, put it into the specs. */
					specs[numspecs].font = font->match;
					specs[numspecs].glyph = shaped.glyphs[code_idx].codepoint;
					specs[numspecs].x = xp + (short)shaped.positions[code_idx].x_offset;
					specs[numspecs].y = yp + (short)shaped.positions[code_idx].y_offset;
					xp += runewidth;
					numspecs++;
				} else {
					/* If it's not found, try to fetch it through the font cache. */
					for (f = 0; f < frclen; f++) {
						glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
						/* Everything correct. */
						if (glyphidx && frc[f].flags == frcflags)
							break;
						/* We got a default font for a not found glyph. */
						if (!glyphidx && frc[f].flags == frcflags
								&& frc[f].unicodep == rune) {
							break;
						}
					}

					/* Nothing was found. Use fontconfig to find matching font. */
					if (f >= frclen) {
						if (!font->set)
							font->set = FcFontSort(0, font->pattern, 1, 0, &fcres);
						fcsets[0] = font->set;

						/*
						 * Nothing was found in the cache. Now use
						 * some dozen of Fontconfig calls to get the
						 * font for one single character.
						 *
						 * Xft and fontconfig are design failures.
						 */
						fcpattern = FcPatternDuplicate(font->pattern);
						fccharset = FcCharSetCreate();

						FcCharSetAddChar(fccharset, rune);
						FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
						FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

						FcConfigSubstitute(0, fcpattern, FcMatchPattern);
						FcDefaultSubstitute(fcpattern);

						fontpattern = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);

						/* Allocate memory for the new cache entry. */
						if (frclen >= frccap) {
							frccap += 16;
							frc = xrealloc(frc, frccap * sizeof(Fontcache));
						}

						frc[frclen].font = XftFontOpenPattern(xw.dpy, fontpattern);
						if (!frc[frclen].font)
							die("XftFontOpenPattern failed seeking fallback font: %s\n",
								strerror(errno));
						frc[frclen].flags = frcflags;
						frc[frclen].unicodep = rune;

						glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

						f = frclen;
						frclen++;

						FcPatternDestroy(fcpattern);
						FcCharSetDestroy(fccharset);
					}

					specs[numspecs].font = frc[f].font;
					specs[numspecs].glyph = glyphidx;
					specs[numspecs].x = (short)xp;
					specs[numspecs].y = (short)yp;
					xp += runewidth;
					numspecs++;
				}
			}

			/* Cleanup and get ready for next segment. */
			hbcleanup(&shaped);
			start = i;

			/* Determine font for glyph if different from previous glyph. */
			if (prevmode != mode) {
				prevmode = mode;
				xresetfontsettings(mode, &font, &frcflags);
				yp = winy + font->ascent;
			}
		}
		#else // !LIGATURES_PATCH
		if (mode == ATTR_WDUMMY)
			continue;

		/* Determine font for glyph if different from previous glyph. */
		if (prevmode != mode) {
			prevmode = mode;
			font = &dc.font;
			frcflags = FRC_NORMAL;
			runewidth = win.cw * ((mode & ATTR_WIDE) ? 2.0f : 1.0f);
			if ((mode & ATTR_ITALIC) && (mode & ATTR_BOLD)) {
				font = &dc.ibfont;
				frcflags = FRC_ITALICBOLD;
			} else if (mode & ATTR_ITALIC) {
				font = &dc.ifont;
				frcflags = FRC_ITALIC;
			} else if (mode & ATTR_BOLD) {
				font = &dc.bfont;
				frcflags = FRC_BOLD;
			}
			#if VERTCENTER_PATCH
			yp = winy + font->ascent + win.cyo;
			#else
			yp = winy + font->ascent;
			#endif // VERTCENTER_PATCH
		}

		#if BOXDRAW_PATCH
		if (mode & ATTR_BOXDRAW) {
			/* minor shoehorning: boxdraw uses only this ushort */
			glyphidx = boxdrawindex(&glyphs[i]);
		} else {
			/* Lookup character index with default font. */
			glyphidx = XftCharIndex(xw.dpy, font->match, rune);
		}
		#else
		/* Lookup character index with default font. */
		glyphidx = XftCharIndex(xw.dpy, font->match, rune);
		#endif // BOXDRAW_PATCH
		if (glyphidx) {
			specs[numspecs].font = font->match;
			specs[numspecs].glyph = glyphidx;
			specs[numspecs].x = (short)xp;
			specs[numspecs].y = (short)yp;
			xp += runewidth;
			numspecs++;
			continue;
		}

		/* Fallback on font cache, search the font cache for match. */
		for (f = 0; f < frclen; f++) {
			glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
			/* Everything correct. */
			if (glyphidx && frc[f].flags == frcflags)
				break;
			/* We got a default font for a not found glyph. */
			if (!glyphidx && frc[f].flags == frcflags
					&& frc[f].unicodep == rune) {
				break;
			}
		}

		/* Nothing was found. Use fontconfig to find matching font. */
		if (f >= frclen) {
			if (!font->set)
				font->set = FcFontSort(0, font->pattern, 1, 0, &fcres);
			fcsets[0] = font->set;

			/*
			 * Nothing was found in the cache. Now use
			 * some dozen of Fontconfig calls to get the
			 * font for one single character.
			 *
			 * Xft and fontconfig are design failures.
			 */
			fcpattern = FcPatternDuplicate(font->pattern);
			fccharset = FcCharSetCreate();

			FcCharSetAddChar(fccharset, rune);
			FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

			#if !USE_XFTFONTMATCH_PATCH
			FcConfigSubstitute(0, fcpattern, FcMatchPattern);
			FcDefaultSubstitute(fcpattern);
			#endif // USE_XFTFONTMATCH_PATCH

			fontpattern = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);

			/* Allocate memory for the new cache entry. */
			if (frclen >= frccap) {
				frccap += 16;
				frc = xrealloc(frc, frccap * sizeof(Fontcache));
			}

			frc[frclen].font = XftFontOpenPattern(xw.dpy, fontpattern);
			if (!frc[frclen].font)
				die("XftFontOpenPattern failed seeking fallback font: %s\n",
					strerror(errno));
			frc[frclen].flags = frcflags;
			frc[frclen].unicodep = rune;

			glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

			f = frclen;
			frclen++;

			FcPatternDestroy(fcpattern);
			FcCharSetDestroy(fccharset);
		}

		specs[numspecs].font = frc[f].font;
		specs[numspecs].glyph = glyphidx;
		specs[numspecs].x = (short)xp;
		specs[numspecs].y = (short)yp;
		xp += runewidth;
		numspecs++;
		#endif // LIGATURES_PATCH
	}

	return numspecs;
}

#if UNDERCURL_PATCH
static int isSlopeRising (int x, int iPoint, int waveWidth)
{
	//    .     .     .     .
	//   / \   / \   / \   / \
	//  /   \ /   \ /   \ /   \
	// .     .     .     .     .

	// Find absolute `x` of point
	x += iPoint * (waveWidth/2);

	// Find index of absolute wave
	int absSlope = x / ((float)waveWidth/2);

	return (absSlope % 2);
}

static int getSlope (int x, int iPoint, int waveWidth)
{
	// Sizes: Caps are half width of slopes
	//    1_2       1_2       1_2      1_2
	//   /   \     /   \     /   \    /   \
	//  /     \   /     \   /     \  /     \
	// 0       3_0       3_0      3_0       3_
	// <2->    <1>         <---6---->

	// Find type of first point
	int firstType;
	x -= (x / waveWidth) * waveWidth;
	if (x < (waveWidth * (2.f/6.f)))
		firstType = UNDERCURL_SLOPE_ASCENDING;
	else if (x < (waveWidth * (3.f/6.f)))
		firstType = UNDERCURL_SLOPE_TOP_CAP;
	else if (x < (waveWidth * (5.f/6.f)))
		firstType = UNDERCURL_SLOPE_DESCENDING;
	else
		firstType = UNDERCURL_SLOPE_BOTTOM_CAP;

	// Find type of given point
	int pointType = (iPoint % 4);
	pointType += firstType;
	pointType %= 4;

	return pointType;
}
#endif // UNDERCURL_PATCH

void
#if WIDE_GLYPHS_PATCH
xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Glyph base, int len, int x, int y, int dmode)
#else
xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Glyph base, int len, int x, int y)
#endif // WIDE_GLYPHS_PATCH
{
	int charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);
	#if ANYSIZE_PATCH
	int winx = win.hborderpx + x * win.cw, winy = win.vborderpx + y * win.ch;
	#else
	int winx = borderpx + x * win.cw, winy = borderpx + y * win.ch;
	#endif // ANYSIZE_PATCH
	int width = charlen * win.cw;
	Color *fg, *bg, *temp, revfg, revbg, truefg, truebg;
	XRenderColor colfg, colbg;
	XRectangle r;

	/* Fallback on color display for attributes not supported by the font */
	if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD) {
		if (dc.ibfont.badslant || dc.ibfont.badweight)
			base.fg = defaultattr;
	} else if ((base.mode & ATTR_ITALIC && dc.ifont.badslant) ||
	    (base.mode & ATTR_BOLD && dc.bfont.badweight)) {
		base.fg = defaultattr;
	}

	if (IS_TRUECOL(base.fg)) {
		colfg.alpha = 0xffff;
		colfg.red = TRUERED(base.fg);
		colfg.green = TRUEGREEN(base.fg);
		colfg.blue = TRUEBLUE(base.fg);
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &truefg);
		fg = &truefg;
	} else {
		fg = &dc.col[base.fg];
	}

	if (IS_TRUECOL(base.bg)) {
		colbg.alpha = 0xffff;
		colbg.green = TRUEGREEN(base.bg);
		colbg.red = TRUERED(base.bg);
		colbg.blue = TRUEBLUE(base.bg);
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &truebg);
		bg = &truebg;
	} else {
		bg = &dc.col[base.bg];
	}

	#if !BOLD_IS_NOT_BRIGHT_PATCH
	/* Change basic system colors [0-7] to bright system colors [8-15] */
	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base.fg, 0, 7))
		fg = &dc.col[base.fg + 8];
	#endif // BOLD_IS_NOT_BRIGHT_PATCH

	if (IS_SET(MODE_REVERSE)) {
		if (fg == &dc.col[defaultfg]) {
			fg = &dc.col[defaultbg];
		} else {
			colfg.red = ~fg->color.red;
			colfg.green = ~fg->color.green;
			colfg.blue = ~fg->color.blue;
			colfg.alpha = fg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg,
					&revfg);
			fg = &revfg;
		}

		if (bg == &dc.col[defaultbg]) {
			bg = &dc.col[defaultfg];
		} else {
			colbg.red = ~bg->color.red;
			colbg.green = ~bg->color.green;
			colbg.blue = ~bg->color.blue;
			colbg.alpha = bg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg,
					&revbg);
			bg = &revbg;
		}
	}

	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		colfg.red = fg->color.red / 2;
		colfg.green = fg->color.green / 2;
		colfg.blue = fg->color.blue / 2;
		colfg.alpha = fg->color.alpha;
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
		fg = &revfg;
	}

	if (base.mode & ATTR_REVERSE) {
		#if SPOILER_PATCH
		if (bg == fg) {
			bg = &dc.col[defaultfg];
			fg = &dc.col[defaultbg];
		} else {
			temp = fg;
			fg = bg;
			bg = temp;
		}
		#else
		temp = fg;
		fg = bg;
		bg = temp;
		#endif // SPOILER_PATCH
	}

	if (base.mode & ATTR_BLINK && win.mode & MODE_BLINK)
		fg = bg;

	if (base.mode & ATTR_INVISIBLE)
		fg = bg;

	#if INVERT_PATCH
	if (invertcolors) {
		revfg = invertedcolor(fg);
		revbg = invertedcolor(bg);
		fg = &revfg;
		bg = &revbg;
	}
	#endif // INVERT_PATCH

	#if ALPHA_PATCH && ALPHA_GRADIENT_PATCH
	// gradient
	bg->color.alpha = grad_alpha * 0xffff * (win.h - y*win.ch) / win.h + stat_alpha * 0xffff;
	// uncomment to invert the gradient
	// bg->color.alpha = grad_alpha * 0xffff * (y*win.ch) / win.h + stat_alpha * 0xffff;
	#endif // ALPHA_PATCH | ALPHA_GRADIENT_PATCH

	#if WIDE_GLYPHS_PATCH
	if (dmode & DRAW_BG) {
	#endif // WIDE_GLYPHS_PATCH
	/* Intelligent cleaning up of the borders. */
	#if ANYSIZE_PATCH
	if (x == 0) {
		xclear(0, (y == 0)? 0 : winy, win.hborderpx,
			winy + win.ch +
			((winy + win.ch >= win.vborderpx + win.th)? win.h : 0));
	}
	if (winx + width >= win.hborderpx + win.tw) {
		xclear(winx + width, (y == 0)? 0 : winy, win.w,
			((winy + win.ch >= win.vborderpx + win.th)? win.h : (winy + win.ch)));
	}
	if (y == 0)
		xclear(winx, 0, winx + width, win.vborderpx);
	if (winy + win.ch >= win.vborderpx + win.th)
		xclear(winx, winy + win.ch, winx + width, win.h);
	#else
	if (x == 0) {
		xclear(0, (y == 0)? 0 : winy, borderpx,
			winy + win.ch +
			((winy + win.ch >= borderpx + win.th)? win.h : 0));
	}
	if (winx + width >= borderpx + win.tw) {
		xclear(winx + width, (y == 0)? 0 : winy, win.w,
			((winy + win.ch >= borderpx + win.th)? win.h : (winy + win.ch)));
	}
	if (y == 0)
		xclear(winx, 0, winx + width, borderpx);
	if (winy + win.ch >= borderpx + win.th)
		xclear(winx, winy + win.ch, winx + width, win.h);
	#endif // ANYSIZE_PATCH

	/* Clean up the region we want to draw to. */
	#if BACKGROUND_IMAGE_PATCH
	if (bg == &dc.col[defaultbg])
		xclear(winx, winy, winx + width, winy + win.ch);
	else
	#endif // BACKGROUND_IMAGE_PATCH

	#if !WIDE_GLYPHS_PATCH
	XftDrawRect(xw.draw, bg, winx, winy, width, win.ch);
	#endif // WIDE_GLYPHS_PATCH

	/* Set the clip region because Xft is sometimes dirty. */
	r.x = 0;
	r.y = 0;
	r.height = win.ch;
	r.width = width;
	XftDrawSetClipRectangles(xw.draw, winx, winy, &r, 1);

	#if WIDE_GLYPHS_PATCH
		/* Fill the background */
		XftDrawRect(xw.draw, bg, winx, winy, width, win.ch);
	}
	#endif // WIDE_GLYPHS_PATCH

	#if WIDE_GLYPHS_PATCH
	if (dmode & DRAW_FG) {
	#endif // WIDE_GLYPHS_PATCH
	#if BOXDRAW_PATCH
	if (base.mode & ATTR_BOXDRAW) {
		drawboxes(winx, winy, width / len, win.ch, fg, bg, specs, len);
	} else {
		/* Render the glyphs. */
		XftDrawGlyphFontSpec(xw.draw, fg, specs, len);
	}
	#else
	/* Render the glyphs. */
	XftDrawGlyphFontSpec(xw.draw, fg, specs, len);
	#endif // BOXDRAW_PATCH

	/* Render underline and strikethrough. */
	if (base.mode & ATTR_UNDERLINE) {
		#if UNDERCURL_PATCH
		// Underline Color
		const int widthThreshold  = 28; // +1 width every widthThreshold px of font
		int wlw = (win.ch / widthThreshold) + 1; // Wave Line Width
		int linecolor;
		if ((base.ucolor[0] >= 0) &&
			!(base.mode & ATTR_BLINK && win.mode & MODE_BLINK) &&
			!(base.mode & ATTR_INVISIBLE)
		) {
			// Special color for underline
			// Index
			if (base.ucolor[1] < 0) {
				linecolor = dc.col[base.ucolor[0]].pixel;
			}
			// RGB
			else {
				XColor lcolor;
				lcolor.red = base.ucolor[0] * 257;
				lcolor.green = base.ucolor[1] * 257;
				lcolor.blue = base.ucolor[2] * 257;
				lcolor.flags = DoRed | DoGreen | DoBlue;
				XAllocColor(xw.dpy, xw.cmap, &lcolor);
				linecolor = lcolor.pixel;
			}
		} else {
			// Foreground color for underline
			linecolor = fg->pixel;
		}

		XGCValues ugcv = {
			.foreground = linecolor,
			.line_width = wlw,
			.line_style = LineSolid,
			.cap_style = CapNotLast
		};

		GC ugc = XCreateGC(xw.dpy, XftDrawDrawable(xw.draw),
			GCForeground | GCLineWidth | GCLineStyle | GCCapStyle,
			&ugcv);

		// Underline Style
		if (base.ustyle != 3) {
			XFillRectangle(xw.dpy, XftDrawDrawable(xw.draw), ugc, winx,
				winy + dc.font.ascent * chscale + 1, width, wlw);
		} else if (base.ustyle == 3) {
			int ww = win.cw;//width;
			int wh = dc.font.descent - wlw/2 - 1;//r.height/7;
			int wx = winx;
			int wy = winy + win.ch - dc.font.descent;
			#if VERTCENTER_PATCH
			wy -= win.cyo;
			#endif // VERTCENTER_PATCH

#if UNDERCURL_STYLE == UNDERCURL_CURLY
			// Draw waves
			int narcs = charlen * 2 + 1;
			XArc *arcs = xmalloc(sizeof(XArc) * narcs);

			int i = 0;
			for (i = 0; i < charlen-1; i++) {
				arcs[i*2] = (XArc) {
					.x = wx + win.cw * i + ww / 4,
					.y = wy,
					.width = win.cw / 2,
					.height = wh,
					.angle1 = 0,
					.angle2 = 180 * 64
				};
				arcs[i*2+1] = (XArc) {
					.x = wx + win.cw * i + ww * 0.75,
					.y = wy,
					.width = win.cw/2,
					.height = wh,
					.angle1 = 180 * 64,
					.angle2 = 180 * 64
				};
			}
			// Last wave
			arcs[i*2] = (XArc) {wx + ww * i + ww / 4, wy, ww / 2, wh,
			0, 180 * 64 };
			// Last wave tail
			arcs[i*2+1] = (XArc) {wx + ww * i + ww * 0.75, wy, ceil(ww / 2.),
			wh, 180 * 64, 90 * 64};
			// First wave tail
			i++;
			arcs[i*2] = (XArc) {wx - ww/4 - 1, wy, ceil(ww / 2.), wh, 270 * 64,
			90 * 64 };

			XDrawArcs(xw.dpy, XftDrawDrawable(xw.draw), ugc, arcs, narcs);

			free(arcs);
#elif UNDERCURL_STYLE == UNDERCURL_SPIKY
			// Make the underline corridor larger
			/*
			wy -= wh;
			*/
			wh *= 2;

			// Set the angle of the slope to 45°
			ww = wh;

			// Position of wave is independent of word, it's absolute
			wx = (wx / (ww/2)) * (ww/2);

			int marginStart = winx - wx;

			// Calculate number of points with floating precision
			float n = width;					// Width of word in pixels
			n = (n / ww) * 2;					// Number of slopes (/ or \)
			n += 2;								// Add two last points
			int npoints = n;					// Convert to int

			// Total length of underline
			float waveLength = 0;

			if (npoints >= 3) {
				// We add an aditional slot in case we use a bonus point
				XPoint *points = xmalloc(sizeof(XPoint) * (npoints + 1));

				// First point (Starts with the word bounds)
				points[0] = (XPoint) {
					.x = wx + marginStart,
					.y = (isSlopeRising(wx, 0, ww))
						? (wy - marginStart + ww/2.f)
						: (wy + marginStart)
				};

				// Second point (Goes back to the absolute point coordinates)
				points[1] = (XPoint) {
					.x = (ww/2.f) - marginStart,
					.y = (isSlopeRising(wx, 1, ww))
						? (ww/2.f - marginStart)
						: (-ww/2.f + marginStart)
				};
				waveLength += (ww/2.f) - marginStart;

				// The rest of the points
				for (int i = 2; i < npoints-1; i++) {
					points[i] = (XPoint) {
						.x = ww/2,
						.y = (isSlopeRising(wx, i, ww))
							? wh/2
							: -wh/2
					};
					waveLength += ww/2;
				}

				// Last point
				points[npoints-1] = (XPoint) {
					.x = ww/2,
					.y = (isSlopeRising(wx, npoints-1, ww))
						? wh/2
						: -wh/2
				};
				waveLength += ww/2;

				// End
				if (waveLength < width) { // Add a bonus point?
					int marginEnd = width - waveLength;
					points[npoints] = (XPoint) {
						.x = marginEnd,
						.y = (isSlopeRising(wx, npoints, ww))
							? (marginEnd)
							: (-marginEnd)
					};

					npoints++;
				} else if (waveLength > width) { // Is last point too far?
					int marginEnd = waveLength - width;
					points[npoints-1].x -= marginEnd;
					if (isSlopeRising(wx, npoints-1, ww))
						points[npoints-1].y -= (marginEnd);
					else
						points[npoints-1].y += (marginEnd);
				}

				// Draw the lines
				XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points, npoints,
						CoordModePrevious);

				// Draw a second underline with an offset of 1 pixel
				if ( ((win.ch / (widthThreshold/2)) % 2)) {
					points[0].x++;

					XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points,
							npoints, CoordModePrevious);
				}

				// Free resources
				free(points);
			}
#else // UNDERCURL_CAPPED
			// Cap is half of wave width
			float capRatio = 0.5f;

			// Make the underline corridor larger
			wh *= 2;

			// Set the angle of the slope to 45°
			ww = wh;
			ww *= 1 + capRatio; // Add a bit of width for the cap

			// Position of wave is independent of word, it's absolute
			wx = (wx / ww) * ww;

			float marginStart;
			switch(getSlope(winx, 0, ww)) {
				case UNDERCURL_SLOPE_ASCENDING:
					marginStart = winx - wx;
					break;
				case UNDERCURL_SLOPE_TOP_CAP:
					marginStart = winx - (wx + (ww * (2.f/6.f)));
					break;
				case UNDERCURL_SLOPE_DESCENDING:
					marginStart = winx - (wx + (ww * (3.f/6.f)));
					break;
				case UNDERCURL_SLOPE_BOTTOM_CAP:
					marginStart = winx - (wx + (ww * (5.f/6.f)));
					break;
			}

			// Calculate number of points with floating precision
			float n = width;					// Width of word in pixels
												//					   ._.
			n = (n / ww) * 4;					// Number of points (./   \.)
			n += 2;								// Add two last points
			int npoints = n;					// Convert to int

			// Position of the pen to draw the lines
			float penX = 0;
			float penY = 0;

			if (npoints >= 3) {
				XPoint *points = xmalloc(sizeof(XPoint) * (npoints + 1));

				// First point (Starts with the word bounds)
				penX = winx;
				switch (getSlope(winx, 0, ww)) {
					case UNDERCURL_SLOPE_ASCENDING:
						penY = wy + wh/2.f - marginStart;
						break;
					case UNDERCURL_SLOPE_TOP_CAP:
						penY = wy;
						break;
					case UNDERCURL_SLOPE_DESCENDING:
						penY = wy + marginStart;
						break;
					case UNDERCURL_SLOPE_BOTTOM_CAP:
						penY = wy + wh/2.f;
						break;
				}
				points[0].x = penX;
				points[0].y = penY;

				// Second point (Goes back to the absolute point coordinates)
				switch (getSlope(winx, 1, ww)) {
					case UNDERCURL_SLOPE_ASCENDING:
						penX += ww * (1.f/6.f) - marginStart;
						penY += 0;
						break;
					case UNDERCURL_SLOPE_TOP_CAP:
						penX += ww * (2.f/6.f) - marginStart;
						penY += -wh/2.f + marginStart;
						break;
					case UNDERCURL_SLOPE_DESCENDING:
						penX += ww * (1.f/6.f) - marginStart;
						penY += 0;
						break;
					case UNDERCURL_SLOPE_BOTTOM_CAP:
						penX += ww * (2.f/6.f) - marginStart;
						penY += -marginStart + wh/2.f;
						break;
				}
				points[1].x = penX;
				points[1].y = penY;

				// The rest of the points
				for (int i = 2; i < npoints; i++) {
					switch (getSlope(winx, i, ww)) {
						case UNDERCURL_SLOPE_ASCENDING:
						case UNDERCURL_SLOPE_DESCENDING:
							penX += ww * (1.f/6.f);
							penY += 0;
							break;
						case UNDERCURL_SLOPE_TOP_CAP:
							penX += ww * (2.f/6.f);
							penY += -wh / 2.f;
							break;
						case UNDERCURL_SLOPE_BOTTOM_CAP:
							penX += ww * (2.f/6.f);
							penY += wh / 2.f;
							break;
					}
					points[i].x = penX;
					points[i].y = penY;
				}

				// End
				float waveLength = penX - winx;
				if (waveLength < width) { // Add a bonus point?
					int marginEnd = width - waveLength;
					penX += marginEnd;
					switch(getSlope(winx, npoints, ww)) {
						case UNDERCURL_SLOPE_ASCENDING:
						case UNDERCURL_SLOPE_DESCENDING:
							//penY += 0;
							break;
						case UNDERCURL_SLOPE_TOP_CAP:
							penY += -marginEnd;
							break;
						case UNDERCURL_SLOPE_BOTTOM_CAP:
							penY += marginEnd;
							break;
					}

					points[npoints].x = penX;
					points[npoints].y = penY;

					npoints++;
				} else if (waveLength > width) { // Is last point too far?
					int marginEnd = waveLength - width;
					points[npoints-1].x -= marginEnd;
					switch(getSlope(winx, npoints-1, ww)) {
						case UNDERCURL_SLOPE_TOP_CAP:
							points[npoints-1].y += marginEnd;
							break;
						case UNDERCURL_SLOPE_BOTTOM_CAP:
							points[npoints-1].y -= marginEnd;
							break;
						default:
							break;
					}
				}

				// Draw the lines
				XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points, npoints,
						CoordModeOrigin);

				// Draw a second underline with an offset of 1 pixel
				if ( ((win.ch / (widthThreshold/2)) % 2)) {
					for (int i = 0; i < npoints; i++)
						points[i].x++;

					XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points,
							npoints, CoordModeOrigin);
				}

				// Free resources
				free(points);
			}
#endif
		}

		XFreeGC(xw.dpy, ugc);
		#elif VERTCENTER_PATCH
		XftDrawRect(xw.draw, fg, winx, winy + win.cyo + dc.font.ascent * chscale + 1,
				width, 1);
		#else
		XftDrawRect(xw.draw, fg, winx, winy + dc.font.ascent * chscale + 1,
				width, 1);
		#endif // UNDERCURL_PATCH | VERTCENTER_PATCH
	}

	if (base.mode & ATTR_STRUCK) {
		#if VERTCENTER_PATCH
		XftDrawRect(xw.draw, fg, winx, winy + win.cyo + 2 * dc.font.ascent * chscale / 3,
				width, 1);
		#else
		XftDrawRect(xw.draw, fg, winx, winy + 2 * dc.font.ascent * chscale / 3,
				width, 1);
		#endif // VERTCENTER_PATCH
	}
	#if WIDE_GLYPHS_PATCH
	}
	#endif // WIDE_GLYPHS_PATCH

	#if OPENURLONCLICK_PATCH
	if (url_draw && y >= url_y1 && y <= url_y2) {
		int x1 = (y == url_y1) ? url_x1 : 0;
		int x2 = (y == url_y2) ? MIN(url_x2, term.col-1) : url_maxcol;
		if (x + charlen > x1 && x <= x2) {
			int xu = MAX(x, x1);
			int wu = (x2 - xu + 1) * win.cw;
			#if ANYSIZE_PATCH
			xu = win.hborderpx + xu * win.cw;
			#else
			xu = borderpx + xu * win.cw;
			#endif // ANYSIZE_PATCH
			#if VERTCENTER_PATCH
			XftDrawRect(xw.draw, fg, xu, winy + win.cyo + dc.font.ascent * chscale + 2, wu, 1);
			#else
			XftDrawRect(xw.draw, fg, xu, winy + dc.font.ascent * chscale + 2, wu, 1);
			#endif // VERTCENTER_PATCH
			url_draw = (y != url_y2 || x + charlen <= x2);
		}
	}
	#endif // OPENURLONCLICK_PATCH

	/* Reset clip to none. */
	XftDrawSetClip(xw.draw, 0);
}

void
xdrawglyph(Glyph g, int x, int y)
{
	int numspecs;
	XftGlyphFontSpec spec;

	numspecs = xmakeglyphfontspecs(&spec, &g, 1, x, y);
	#if WIDE_GLYPHS_PATCH
	xdrawglyphfontspecs(&spec, g, numspecs, x, y, DRAW_BG | DRAW_FG);
	#else
	xdrawglyphfontspecs(&spec, g, numspecs, x, y);
	#endif // WIDE_GLYPHS_PATCH
}

void
#if LIGATURES_PATCH
xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og, Line line, int len)
#else
xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
#endif // LIGATURES_PATCH
{
	Color drawcol;
	#if DYNAMIC_CURSOR_COLOR_PATCH
	XRenderColor colbg;
	#endif // DYNAMIC_CURSOR_COLOR_PATCH

	/* remove the old cursor */
	if (selected(ox, oy))
		og.mode ^= ATTR_REVERSE;
	#if LIGATURES_PATCH
	/* Redraw the line where cursor was previously.
	 * It will restore the ligatures broken by the cursor. */
	xdrawline(line, 0, oy, len);
	#else
	xdrawglyph(og, ox, oy);
	#endif // LIGATURES_PATCH

	#if HIDE_TERMINAL_CURSOR_PATCH
	if (IS_SET(MODE_HIDE) || !IS_SET(MODE_FOCUSED))
		return;
	#else
	if (IS_SET(MODE_HIDE))
		return;
	#endif // HIDE_TERMINAL_CURSOR_PATCH

	/*
	 * Select the right color for the right mode.
	 */
	#if BOXDRAW_PATCH
	g.mode &= ATTR_BOLD|ATTR_ITALIC|ATTR_UNDERLINE|ATTR_STRUCK|ATTR_WIDE|ATTR_BOXDRAW;
	#else
	g.mode &= ATTR_BOLD|ATTR_ITALIC|ATTR_UNDERLINE|ATTR_STRUCK|ATTR_WIDE;
	#endif // BOXDRAW_PATCH

	if (IS_SET(MODE_REVERSE)) {
		g.mode |= ATTR_REVERSE;
		g.bg = defaultfg;
		if (selected(cx, cy)) {
			drawcol = dc.col[defaultcs];
			g.fg = defaultrcs;
		} else {
			drawcol = dc.col[defaultrcs];
			g.fg = defaultcs;
		}
	} else {
		if (selected(cx, cy)) {
			g.fg = defaultfg;
			g.bg = defaultrcs;
		}
		#if !DYNAMIC_CURSOR_COLOR_PATCH
		else {
			g.fg = defaultbg;
			g.bg = defaultcs;
		}

		drawcol = dc.col[g.bg];
		#else
		else if (!(og.mode & ATTR_REVERSE)) {
			unsigned int tmpcol = g.bg;
			g.bg = g.fg;
			g.fg = tmpcol;
		}

		if (IS_TRUECOL(g.bg)) {
			colbg.alpha = 0xffff;
			colbg.red = TRUERED(g.bg);
			colbg.green = TRUEGREEN(g.bg);
			colbg.blue = TRUEBLUE(g.bg);
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &drawcol);
		} else
			drawcol = dc.col[g.bg];
		#endif // DYNAMIC_CURSOR_COLOR_PATCH
	}

	/* draw the new one */
	if (IS_SET(MODE_FOCUSED)) {
		switch (win.cursor) {
		#if !BLINKING_CURSOR_PATCH
		case 7: /* st extension */
			g.u = 0x2603; /* snowman (U+2603) */
			/* FALLTHROUGH */
		#endif // BLINKING_CURSOR_PATCH
		case 0: /* Blinking block */
		case 1: /* Blinking block (default) */
			#if BLINKING_CURSOR_PATCH
			if (IS_SET(MODE_BLINK))
				break;
			/* FALLTHROUGH */
			#endif // BLINKING_CURSOR_PATCH
		case 2: /* Steady block */
			xdrawglyph(g, cx, cy);
			break;
		case 3: /* Blinking underline */
			#if BLINKING_CURSOR_PATCH
			if (IS_SET(MODE_BLINK))
				break;
			/* FALLTHROUGH */
			#endif // BLINKING_CURSOR_PATCH
		case 4: /* Steady underline */
			#if ANYSIZE_PATCH
			XftDrawRect(xw.draw, &drawcol,
					win.hborderpx + cx * win.cw,
					win.vborderpx + (cy + 1) * win.ch - \
						cursorthickness,
					win.cw, cursorthickness);
			#else
			XftDrawRect(xw.draw, &drawcol,
					borderpx + cx * win.cw,
					borderpx + (cy + 1) * win.ch - \
						cursorthickness,
					win.cw, cursorthickness);
			#endif // ANYSIZE_PATCH
			break;
		case 5: /* Blinking bar */
			#if BLINKING_CURSOR_PATCH
			if (IS_SET(MODE_BLINK))
				break;
			/* FALLTHROUGH */
			#endif // BLINKING_CURSOR_PATCH
		case 6: /* Steady bar */
			XftDrawRect(xw.draw, &drawcol,
					#if ANYSIZE_PATCH
					win.hborderpx + cx * win.cw,
					win.vborderpx + cy * win.ch,
					#else
					borderpx + cx * win.cw,
					borderpx + cy * win.ch,
					#endif // ANYSIZE_PATCH
					cursorthickness, win.ch);
			break;
		#if BLINKING_CURSOR_PATCH
		case 7: /* Blinking st cursor */
			if (IS_SET(MODE_BLINK))
				break;
			/* FALLTHROUGH */
		case 8: /* Steady st cursor */
			g.u = stcursor;
			xdrawglyph(g, cx, cy);
			break;
		#endif // BLINKING_CURSOR_PATCH
		}
	} else {
		XftDrawRect(xw.draw, &drawcol,
				#if ANYSIZE_PATCH
				win.hborderpx + cx * win.cw,
				win.vborderpx + cy * win.ch,
				#else
				borderpx + cx * win.cw,
				borderpx + cy * win.ch,
				#endif // ANYSIZE_PATCH
				win.cw - 1, 1);
		XftDrawRect(xw.draw, &drawcol,
				#if ANYSIZE_PATCH
				win.hborderpx + cx * win.cw,
				win.vborderpx + cy * win.ch,
				#else
				borderpx + cx * win.cw,
				borderpx + cy * win.ch,
				#endif // ANYSIZE_PATCH
				1, win.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
				#if ANYSIZE_PATCH
				win.hborderpx + (cx + 1) * win.cw - 1,
				win.vborderpx + cy * win.ch,
				#else
				borderpx + (cx + 1) * win.cw - 1,
				borderpx + cy * win.ch,
				#endif // ANYSIZE_PATCH
				1, win.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
				#if ANYSIZE_PATCH
				win.hborderpx + cx * win.cw,
				win.vborderpx + (cy + 1) * win.ch - 1,
				#else
				borderpx + cx * win.cw,
				borderpx + (cy + 1) * win.ch - 1,
				#endif // ANYSIZE_PATCH
				win.cw, 1);
	}
}

void
xsetenv(void)
{
	char buf[sizeof(long) * 8 + 1];

	snprintf(buf, sizeof(buf), "%lu", xw.win);
	setenv("WINDOWID", buf, 1);
}

void
xseticontitle(char *p)
{
	XTextProperty prop;
	DEFAULT(p, opt_title);

	if (Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
			&prop) != Success)
		return;
	XSetWMIconName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmiconname);
	XFree(prop.value);
}

#if CSI_22_23_PATCH
void
xsettitle(char *p, int pop)
{
	XTextProperty prop;

	free(titlestack[tstki]);
	if (pop) {
		titlestack[tstki] = NULL;
		tstki = (tstki - 1 + TITLESTACKSIZE) % TITLESTACKSIZE;
		p = titlestack[tstki] ? titlestack[tstki] : opt_title;
	} else if (p) {
		titlestack[tstki] = xstrdup(p);
	} else {
		titlestack[tstki] = NULL;
		p = opt_title;
	}

	if (Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
			&prop) != Success)
		return;
	XSetWMName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
	XFree(prop.value);
}

void
xpushtitle(void)
{
	int tstkin = (tstki + 1) % TITLESTACKSIZE;

	free(titlestack[tstkin]);
	titlestack[tstkin] = titlestack[tstki] ? xstrdup(titlestack[tstki]) : NULL;
	tstki = tstkin;
}

void
xfreetitlestack(void)
{
	for (int i = 0; i < LEN(titlestack); i++) {
		free(titlestack[i]);
		titlestack[i] = NULL;
	}
}
#else
void
xsettitle(char *p)
{
	XTextProperty prop;
	DEFAULT(p, opt_title);

	if (Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
			&prop) != Success)
		return;
	XSetWMName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
	XFree(prop.value);
}
#endif // CSI_22_23_PATCH

int
xstartdraw(void)
{
	#if W3M_PATCH
	if (IS_SET(MODE_VISIBLE))
		XCopyArea(xw.dpy, xw.win, xw.buf, dc.gc, 0, 0, win.w, win.h, 0, 0);
	#endif // W3M_PATCH
	return IS_SET(MODE_VISIBLE);
}

void
xdrawline(Line line, int x1, int y1, int x2)
{
	int i, x, ox, numspecs;
	#if WIDE_GLYPHS_PATCH
	int numspecs_cached;
	#endif // WIDE_GLYPHS_PATCH
	Glyph base, new;
	#if WIDE_GLYPHS_PATCH
	XftGlyphFontSpec *specs;

	numspecs_cached = xmakeglyphfontspecs(xw.specbuf, &line[x1], x2 - x1, x1, y1);

	/* Draw line in 2 passes: background and foreground. This way wide glyphs
	   won't get truncated (#223) */
	for (int dmode = DRAW_BG; dmode <= DRAW_FG; dmode <<= 1) {
		specs = xw.specbuf;
		numspecs = numspecs_cached;
		i = ox = 0;
		for (x = x1; x < x2 && i < numspecs; x++) {
			new = line[x];
			#if VIM_BROWSE_PATCH
			historyOverlay(x, y1, &new);
			#endif // VIM_BROWSE_PATCH
			if (new.mode == ATTR_WDUMMY)
				continue;
			if (selected(x, y1))
				new.mode ^= ATTR_REVERSE;
			if (i > 0 && ATTRCMP(base, new)) {
				xdrawglyphfontspecs(specs, base, i, ox, y1, dmode);
				specs += i;
				numspecs -= i;
				i = 0;
			}
			if (i == 0) {
				ox = x;
				base = new;
			}
			i++;
		}
		if (i > 0)
			xdrawglyphfontspecs(specs, base, i, ox, y1, dmode);
	}
	#else
	XftGlyphFontSpec *specs = xw.specbuf;

	numspecs = xmakeglyphfontspecs(specs, &line[x1], x2 - x1, x1, y1);
	i = ox = 0;
	for (x = x1; x < x2 && i < numspecs; x++) {
		new = line[x];
		#if VIM_BROWSE_PATCH
		historyOverlay(x, y1, &new);
		#endif // VIM_BROWSE_PATCH
		if (new.mode == ATTR_WDUMMY)
			continue;
		if (selected(x, y1))
			new.mode ^= ATTR_REVERSE;
		if (i > 0 && ATTRCMP(base, new)) {
			xdrawglyphfontspecs(specs, base, i, ox, y1);
			specs += i;
			numspecs -= i;
			i = 0;
		}
		if (i == 0) {
			ox = x;
			base = new;
		}
		i++;
	}
	if (i > 0)
		xdrawglyphfontspecs(specs, base, i, ox, y1);
	#endif // WIDE_GLYPHS_PATCH
}

void
xfinishdraw(void)
{
	#if SIXEL_PATCH
	ImageList *im, *next;
	XGCValues gcvalues;
	GC gc;
	#endif // SIXEL_PATCH

	#if SIXEL_PATCH
	for (im = term.images; im; im = next) {
		/* get the next image here, because delete_image() will delete the current image */
		next = im->next;

		if (im->should_delete) {
			delete_image(im);
			continue;
		}

		if (!im->pixmap) {
			im->pixmap = (void *)XCreatePixmap(xw.dpy, xw.win, im->width, im->height,
				#if ALPHA_PATCH
				xw.depth
				#else
				DefaultDepth(xw.dpy, xw.scr)
				#endif // ALPHA_PATCH
			);
			XImage ximage = {
				.format = ZPixmap,
				.data = (char *)im->pixels,
				.width = im->width,
				.height = im->height,
				.xoffset = 0,
				.byte_order = LSBFirst,
				.bitmap_bit_order = MSBFirst,
				.bits_per_pixel = 32,
				.bytes_per_line = im->width * 4,
				.bitmap_unit = 32,
				.bitmap_pad = 32,
				#if ALPHA_PATCH
				.depth = xw.depth
				#else
				.depth = 24
				#endif // ALPHA_PATCH
			};
			XPutImage(xw.dpy, (Drawable)im->pixmap, dc.gc, &ximage, 0, 0, 0, 0, im->width, im->height);
			free(im->pixels);
			im->pixels = NULL;
		}

		memset(&gcvalues, 0, sizeof(gcvalues));
		gcvalues.graphics_exposures = False;
		gc = XCreateGC(xw.dpy, xw.win, GCGraphicsExposures, &gcvalues);

		#if ANYSIZE_PATCH
		XCopyArea(xw.dpy, (Drawable)im->pixmap, xw.buf, gc, 0, 0, im->width, im->height, win.hborderpx + im->x * win.cw, win.vborderpx + im->y * win.ch);
		#else
		XCopyArea(xw.dpy, (Drawable)im->pixmap, xw.buf, gc, 0, 0, im->width, im->height, borderpx + im->x * win.cw, borderpx + im->y * win.ch);
		#endif // ANYSIZE_PATCH
		XFreeGC(xw.dpy, gc);

	}
	#endif // SIXEL_PATCH

	#if !SINGLE_DRAWABLE_BUFFER_PATCH
	XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, win.w, win.h, 0, 0);
	#endif // SINGLE_DRAWABLE_BUFFER_PATCH
	XSetForeground(xw.dpy, dc.gc,
			dc.col[IS_SET(MODE_REVERSE)?
				defaultfg : defaultbg].pixel);
}

void
xximspot(int x, int y)
{
	if (xw.ime.xic == NULL)
		return;

	xw.ime.spot.x = borderpx + x * win.cw;
	xw.ime.spot.y = borderpx + (y + 1) * win.ch;

	XSetICValues(xw.ime.xic, XNPreeditAttributes, xw.ime.spotlist, NULL);
}

void
expose(XEvent *ev)
{
	redraw();
}

void
visibility(XEvent *ev)
{
	XVisibilityEvent *e = &ev->xvisibility;

	MODBIT(win.mode, e->state != VisibilityFullyObscured, MODE_VISIBLE);
}

void
unmap(XEvent *ev)
{
	#if ST_EMBEDDER_PATCH
	if (embed == ev->xunmap.window) {
		embed = 0;
		XRaiseWindow(xw.dpy, xw.win);
		XSetInputFocus(xw.dpy, xw.win, RevertToParent, CurrentTime);
	}
	#endif // ST_EMBEDDER_PATCH
	win.mode &= ~MODE_VISIBLE;
}

void
xsetpointermotion(int set)
{
	#if HIDECURSOR_PATCH
	if (!set && !xw.pointerisvisible)
		return;
	#endif // HIDECURSOR_PATCH
	#if OPENURLONCLICK_PATCH
	set = 1; /* keep MotionNotify event enabled */
	#endif // OPENURLONCLICK_PATCH
	MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
	XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
}

void
xsetmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	#if SWAPMOUSE_PATCH
	if ((flags & MODE_MOUSE)
	#if HIDECURSOR_PATCH
		&& xw.pointerisvisible
	#endif // HIDECURSOR_PATCH
	) {
		if (win.mode & MODE_MOUSE)
			XUndefineCursor(xw.dpy, xw.win);
		else
			#if HIDECURSOR_PATCH
			XDefineCursor(xw.dpy, xw.win, xw.vpointer);
			#else
			XDefineCursor(xw.dpy, xw.win, cursor);
			#endif // HIDECURSOR_PATCH
	}
	#elif OPENURLONCLICK_PATCH
	if (win.mode & MODE_MOUSE && xw.pointerisvisible)
		XDefineCursor(xw.dpy, xw.win, xw.vpointer);
	#endif // SWAPMOUSE_PATCH
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
		redraw();
}

int
xsetcursor(int cursor)
{
	#if BLINKING_CURSOR_PATCH
	if (!BETWEEN(cursor, 0, 8)) /* 7-8: st extensions */
	#else
	if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
	#endif // BLINKING_CURSOR_PATCH
		return 1;
	#if DEFAULT_CURSOR_PATCH
	#if BLINKING_CURSOR_PATCH
	win.cursor = (cursor ? cursor : cursorstyle);
	#else
	win.cursor = (cursor ? cursor : cursorshape);
	#endif // BLINKING_CURSOR_PATCH
	#else
	win.cursor = cursor;
	#endif // DEFAULT_CURSOR_PATCH
	#if BLINKING_CURSOR_PATCH
	cursorblinks = win.cursor == 0 || win.cursor == 1 ||
	               win.cursor == 3 || win.cursor == 5 ||
	               win.cursor == 7;
	#endif // BLINKING_CURSOR_PATCH
	return 0;
}

void
xseturgency(int add)
{
	XWMHints *h = XGetWMHints(xw.dpy, xw.win);

	MODBIT(h->flags, add, XUrgencyHint);
	XSetWMHints(xw.dpy, xw.win, h);
	XFree(h);
}

void
xbell(void)
{
	if (!(IS_SET(MODE_FOCUSED)))
		xseturgency(1);
	if (bellvolume)
		XkbBell(xw.dpy, xw.win, bellvolume, (Atom)NULL);
	#if VISUALBELL_1_PATCH
	if (!bellon) /* turn visual bell on */
		bellon = 1;
	#endif // VISUALBELL_1_PATCH
}

void
focus(XEvent *ev)
{
	XFocusChangeEvent *e = &ev->xfocus;

	#if ST_EMBEDDER_PATCH
	if (embed && ev->type == FocusIn) {
		XRaiseWindow(xw.dpy, embed);
		XSetInputFocus(xw.dpy, embed, RevertToParent, CurrentTime);
		sendxembed(XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0);
		sendxembed(XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
	}
	#endif // ST_EMBEDDER_PATCH

	if (e->mode == NotifyGrab)
		return;

	if (ev->type == FocusIn) {
		if (xw.ime.xic)
			XSetICFocus(xw.ime.xic);
		win.mode |= MODE_FOCUSED;
		xseturgency(0);
		if (IS_SET(MODE_FOCUS))
			ttywrite("\033[I", 3, 0);
		#if ALPHA_PATCH && ALPHA_FOCUS_HIGHLIGHT_PATCH
		if (!focused) {
			focused = 1;
			xloadcols();
			tfulldirt();
		}
		#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH
	} else {
		if (xw.ime.xic)
			XUnsetICFocus(xw.ime.xic);
		win.mode &= ~MODE_FOCUSED;
		if (IS_SET(MODE_FOCUS))
			ttywrite("\033[O", 3, 0);
		#if ALPHA_PATCH && ALPHA_FOCUS_HIGHLIGHT_PATCH
		if (focused) {
			focused = 0;
			xloadcols();
			tfulldirt();
		}
		#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH
	}
}

int
match(uint mask, uint state)
{
	return mask == XK_ANY_MOD || mask == (state & ~ignoremod);
}

char*
kmap(KeySym k, uint state)
{
	Key *kp;
	int i;

	/* Check for mapped keys out of X11 function keys. */
	for (i = 0; i < LEN(mappedkeys); i++) {
		if (mappedkeys[i] == k)
			break;
	}
	if (i == LEN(mappedkeys)) {
		if ((k & 0xFFFF) < 0xFD00)
			return NULL;
	}

	for (kp = key; kp < key + LEN(key); kp++) {
		if (kp->k != k)
			continue;

		if (!match(kp->mask, state))
			continue;

		if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0)
			continue;
		if (IS_SET(MODE_NUMLOCK) && kp->appkey == 2)
			continue;

		if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < 0 : kp->appcursor > 0)
			continue;

		return kp->s;
	}

	return NULL;
}

void
kpress(XEvent *ev)
{
	XKeyEvent *e = &ev->xkey;
	KeySym ksym = NoSymbol;
	char buf[64], *customkey;
	int len, screen;
	Rune c;
	Status status;
	Shortcut *bp;

	#if HIDECURSOR_PATCH
	if (xw.pointerisvisible) {
		#if OPENURLONCLICK_PATCH
		#if ANYSIZE_PATCH
		int x = e->x - win.hborderpx;
		int y = e->y - win.vborderpx;
		#else
		int x = e->x - borderpx;
		int y = e->y - borderpx;
		#endif // ANYSIZE_PATCH
		LIMIT(x, 0, win.tw - 1);
		LIMIT(y, 0, win.th - 1);
		if (!detecturl(x / win.cw, y / win.ch, 0)) {
			XDefineCursor(xw.dpy, xw.win, xw.bpointer);
			xsetpointermotion(1);
			xw.pointerisvisible = 0;
		}
		#else
		XDefineCursor(xw.dpy, xw.win, xw.bpointer);
		xsetpointermotion(1);
		xw.pointerisvisible = 0;
		#endif // OPENURLONCLICK_PATCH
	}
	#endif // HIDECURSOR_PATCH

	if (IS_SET(MODE_KBDLOCK))
		return;

	if (xw.ime.xic) {
		len = XmbLookupString(xw.ime.xic, e, buf, sizeof buf, &ksym, &status);
		if (status == XBufferOverflow)
			return;
	} else {
		len = XLookupString(e, buf, sizeof buf, &ksym, NULL);
	}
	#if KEYBOARDSELECT_PATCH
	if ( IS_SET(MODE_KBDSELECT) ) {
		if ( match(XK_NO_MOD, e->state) ||
			(XK_Shift_L | XK_Shift_R) & e->state )
			win.mode ^= trt_kbdselect(ksym, buf, len);
		return;
	}
	#endif // KEYBOARDSELECT_PATCH
	#if VIM_BROWSE_PATCH
	if (IS_SET(MODE_NORMAL)) {
		if (kPressHist(buf, len, match(ControlMask, e->state), &ksym)
		                                      == finish) normalMode();
		return;
	}
	#endif // VIM_BROWSE_PATCH

	screen = tisaltscr() ? S_ALT : S_PRI;

	/* 1. shortcuts */
	for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
		if (ksym == bp->keysym && match(bp->mod, e->state) &&
				(!bp->screen || bp->screen == screen)) {
			bp->func(&(bp->arg));
			return;
		}
	}

	/* 2. custom keys from config.h */
	if ((customkey = kmap(ksym, e->state))) {
		ttywrite(customkey, strlen(customkey), 1);
		return;
	}

	/* 3. composed string from input method */
	if (len == 0)
		return;
	if (len == 1 && e->state & Mod1Mask) {
		if (IS_SET(MODE_8BIT)) {
			if (*buf < 0177) {
				c = *buf | 0x80;
				len = utf8encode(c, buf);
			}
		} else {
			buf[1] = buf[0];
			buf[0] = '\033';
			len = 2;
		}
	}
	ttywrite(buf, len, 1);
}


void
cmessage(XEvent *e)
{
	/*
	 * See xembed specs
	 *  http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
	 */
	if (e->xclient.message_type == xw.xembed && e->xclient.format == 32) {
		if (e->xclient.data.l[1] == XEMBED_FOCUS_IN) {
			win.mode |= MODE_FOCUSED;
			xseturgency(0);
		} else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
			win.mode &= ~MODE_FOCUSED;
		}
	} else if (e->xclient.data.l[0] == xw.wmdeletewin) {
		ttyhangup();
		exit(0);
	}
}

void
resize(XEvent *e)
{
	#if ST_EMBEDDER_PATCH
	XWindowChanges wc;
	#endif // ST_EMBEDDER_PATCH

	#if BACKGROUND_IMAGE_PATCH
	if (pseudotransparency) {
		if (e->xconfigure.width == win.w &&
			e->xconfigure.height == win.h &&
			e->xconfigure.x == win.x && e->xconfigure.y == win.y)
			return;
		updatexy();
	} else
	#endif // BACKGROUND_IMAGE_PATCH
	if (e->xconfigure.width == win.w && e->xconfigure.height == win.h)
		return;

	#if ST_EMBEDDER_PATCH
	if (embed) {
		wc.width = e->xconfigure.width;
		wc.height = e->xconfigure.height;
		XConfigureWindow(xw.dpy, embed, CWWidth | CWHeight, &wc);
	}
	#endif // ST_EMBEDDER_PATCH

	cresize(e->xconfigure.width, e->xconfigure.height);
}

void
run(void)
{
	XEvent ev;
	int w = win.w, h = win.h;
	fd_set rfd;
	int xfd = XConnectionNumber(xw.dpy), ttyfd, xev, drawing;
	struct timespec seltv, *tv, now, lastblink, trigger;
	double timeout;

	/* Waiting for window mapping */
	do {
		XNextEvent(xw.dpy, &ev);
		/*
		 * This XFilterEvent call is required because of XOpenIM. It
		 * does filter out the key event and some client message for
		 * the input method too.
		 */
		if (XFilterEvent(&ev, None))
			continue;
		if (ev.type == ConfigureNotify) {
			w = ev.xconfigure.width;
			h = ev.xconfigure.height;
		}
	} while (ev.type != MapNotify);

	ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);
	cresize(w, h);

	for (timeout = -1, drawing = 0, lastblink = (struct timespec){0};;) {
		FD_ZERO(&rfd);
		FD_SET(ttyfd, &rfd);
		FD_SET(xfd, &rfd);

		#if SYNC_PATCH
		if (XPending(xw.dpy) || ttyread_pending())
		#else
		if (XPending(xw.dpy))
		#endif // SYNC_PATCH
			timeout = 0;  /* existing events might not set xfd */

		seltv.tv_sec = timeout / 1E3;
		seltv.tv_nsec = 1E6 * (timeout - 1E3 * seltv.tv_sec);
		tv = timeout >= 0 ? &seltv : NULL;

		if (pselect(MAX(xfd, ttyfd)+1, &rfd, NULL, NULL, tv, NULL) < 0) {
			if (errno == EINTR)
				continue;
			die("select failed: %s\n", strerror(errno));
		}
		clock_gettime(CLOCK_MONOTONIC, &now);

		#if SYNC_PATCH
		int ttyin = FD_ISSET(ttyfd, &rfd) || ttyread_pending();
		if (ttyin)
			ttyread();
		#else
		if (FD_ISSET(ttyfd, &rfd))
			ttyread();
		#endif // SYNC_PATCH

		xev = 0;
		while (XPending(xw.dpy)) {
			xev = 1;
			XNextEvent(xw.dpy, &ev);
			if (XFilterEvent(&ev, None))
				continue;
			if (handler[ev.type])
				(handler[ev.type])(&ev);
		}

		/*
		 * To reduce flicker and tearing, when new content or event
		 * triggers drawing, we first wait a bit to ensure we got
		 * everything, and if nothing new arrives - we draw.
		 * We start with trying to wait minlatency ms. If more content
		 * arrives sooner, we retry with shorter and shorter periods,
		 * and eventually draw even without idle after maxlatency ms.
		 * Typically this results in low latency while interacting,
		 * maximum latency intervals during `cat huge.txt`, and perfect
		 * sync with periodic updates from animations/key-repeats/etc.
		 */
		#if SYNC_PATCH
		if (ttyin || xev)
		#else
		if (FD_ISSET(ttyfd, &rfd) || xev)
		#endif // SYNC_PATCH
		{
			if (!drawing) {
				trigger = now;
				#if BLINKING_CURSOR_PATCH
				if (IS_SET(MODE_BLINK)) {
					win.mode ^= MODE_BLINK;
				}
				lastblink = now;
				#endif // BLINKING_CURSOR_PATCH
				drawing = 1;
			}
			timeout = (maxlatency - TIMEDIFF(now, trigger)) \
			          / maxlatency * minlatency;
			if (timeout > 0)
				continue;  /* we have time, try to find idle */
		}

		#if SYNC_PATCH
		if (tinsync(su_timeout)) {
			/*
			 * on synchronized-update draw-suspension: don't reset
			 * drawing so that we draw ASAP once we can (just after
			 * ESU). it won't be too soon because we already can
			 * draw now but we skip. we set timeout > 0 to draw on
			 * SU-timeout even without new content.
			 */
			timeout = minlatency;
			continue;
		}
		#endif // SYNC_PATCH

		/* idle detected or maxlatency exhausted -> draw */
		timeout = -1;
		#if BLINKING_CURSOR_PATCH
		if (blinktimeout && (cursorblinks || tattrset(ATTR_BLINK)))
		#else
		if (blinktimeout && tattrset(ATTR_BLINK))
		#endif // BLINKING_CURSOR_PATCH
		{
			timeout = blinktimeout - TIMEDIFF(now, lastblink);
			if (timeout <= 0) {
				if (-timeout > blinktimeout) /* start visible */
					win.mode |= MODE_BLINK;
				win.mode ^= MODE_BLINK;
				tsetdirtattr(ATTR_BLINK);
				lastblink = now;
				timeout = blinktimeout;
			}
		}

		#if VISUALBELL_1_PATCH
		if (bellon) {
			bellon++;
			bellon %= 3;
			MODBIT(win.mode, !IS_SET(MODE_REVERSE), MODE_REVERSE);
			redraw();
		}
		else
			draw();
		#else
		draw();
		#endif // VISUALBELL_1_PATCH
		XFlush(xw.dpy);
		drawing = 0;
	}
}

void
usage(void)
{
	die("usage: %s [-aiv] [-c class]"
		#if WORKINGDIR_PATCH
		" [-d path]"
		#endif // WORKINGDIR_PATCH
		" [-f font] [-g geometry]"
	    " [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid]"
	    " [[-e] command [args ...]]\n"
	    "       %s [-aiv] [-c class]"
		#if WORKINGDIR_PATCH
		" [-d path]"
		#endif // WORKINGDIR_PATCH
		" [-f font] [-g geometry]"
	    " [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid] -l line"
	    " [stty_args ...]\n", argv0, argv0);
}

int
main(int argc, char *argv[])
{
	xw.l = xw.t = 0;
	xw.isfixed = False;
	#if BLINKING_CURSOR_PATCH
	xsetcursor(cursorstyle);
	#else
	xsetcursor(cursorshape);
	#endif // BLINKING_CURSOR_PATCH

	ARGBEGIN {
	case 'a':
		allowaltscreen = 0;
		break;
	#if ALPHA_PATCH
	case 'A':
		opt_alpha = EARGF(usage());
		break;
	#endif // ALPHA_PATCH
	case 'c':
		opt_class = EARGF(usage());
		break;
	#if WORKINGDIR_PATCH
	case 'd':
		opt_dir = EARGF(usage());
		break;
	#endif // WORKINGDIR_PATCH
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		goto run;
	case 'f':
		opt_font = EARGF(usage());
		break;
	case 'g':
		xw.gm = XParseGeometry(EARGF(usage()),
				&xw.l, &xw.t, &cols, &rows);
		break;
	case 'i':
		xw.isfixed = 1;
		break;
	case 'o':
		opt_io = EARGF(usage());
		break;
	case 'l':
		opt_line = EARGF(usage());
		break;
	case 'n':
		opt_name = EARGF(usage());
		break;
	case 't':
	case 'T':
		opt_title = EARGF(usage());
		break;
	case 'w':
		opt_embed = EARGF(usage());
		break;
	case 'v':
		die("%s " VERSION "\n", argv0);
		break;
	default:
		usage();
	} ARGEND;

run:
	if (argc > 0) /* eat all remaining arguments */
		opt_cmd = argv;

	if (!opt_title)
		opt_title = (opt_line || !opt_cmd) ? "st" : opt_cmd[0];

	setlocale(LC_CTYPE, "");
	XSetLocaleModifiers("");
	#if XRESOURCES_PATCH && XRESOURCES_RELOAD_PATCH || BACKGROUND_IMAGE_PATCH && BACKGROUND_IMAGE_RELOAD_PATCH
	signal(SIGUSR1, sigusr1_reload);
	#endif // XRESOURCES_RELOAD_PATCH | BACKGROUND_IMAGE_RELOAD_PATCH
	#if XRESOURCES_PATCH
	if (!(xw.dpy = XOpenDisplay(NULL)))
		die("Can't open display\n");

	config_init(xw.dpy);
	#endif // XRESOURCES_PATCH
	cols = MAX(cols, 1);
	rows = MAX(rows, 1);
	#if ALPHA_PATCH && ALPHA_FOCUS_HIGHLIGHT_PATCH
	defaultbg = MAX(LEN(colorname), 256);
	#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH
	tnew(cols, rows);
	xinit(cols, rows);
	#if BACKGROUND_IMAGE_PATCH
	bginit();
	#endif // BACKGROUND_IMAGE_PATCH
	xsetenv();
	selinit();
	#if WORKINGDIR_PATCH
	if (opt_dir && chdir(opt_dir))
		die("Can't change to working directory %s\n", opt_dir);
	#endif // WORKINGDIR_PATCH
	run();

	return 0;
}
