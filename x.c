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

static char *argv0;
#include "arg.h"
#include "st.h"
#include "win.h"

#if THEMED_CURSOR_PATCH
#include <X11/Xcursor/Xcursor.h>
#endif // THEMED_CURSOR_PATCH

/* types used in config.h */
typedef struct {
	uint mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Shortcut;

typedef struct {
	uint mod;
	uint button;
	void (*func)(const Arg *);
	const Arg arg;
	uint  release;
} MouseShortcut;

typedef struct {
	KeySym k;
	uint mask;
	char *s;
	/* three-valued logic variables: 0 indifferent, 1 on, -1 off */
	signed char appkey;    /* application keypad */
	signed char appcursor; /* application cursor */
} Key;

/* X modifiers */
#define XK_ANY_MOD    UINT_MAX
#define XK_NO_MOD     0
#define XK_SWITCH_MOD (1<<13)

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

/* XEMBED messages */
#define XEMBED_FOCUS_IN  4
#define XEMBED_FOCUS_OUT 5

/* macros */
#define IS_SET(flag)		((win.mode & (flag)) != 0)
#define TRUERED(x)		(((x) & 0xff0000) >> 8)
#define TRUEGREEN(x)		(((x) & 0xff00))
#define TRUEBLUE(x)		(((x) & 0xff) << 8)

typedef XftDraw *Draw;
typedef XftColor Color;
typedef XftGlyphFontSpec GlyphFontSpec;

/* Purely graphic info */
typedef struct {
	int tw, th; /* tty width and height */
	int w, h; /* window width and height */
	#if ANYSIZE_PATCH
	int hborderpx, vborderpx;
	#endif // ANYSIZE_PATCH
	int ch; /* char height */
	int cw; /* char width  */
	#if VERTCENTER_PATCH
	int cyo; /* char y offset */
	#endif // VERTCENTER_PATCH
	int mode; /* window state/mode flags */
	int cursor; /* cursor style */
	#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
	int vbellset; /* 1 during visual bell, 0 otherwise */
	struct timespec lastvbell;
	#endif // VISUALBELL_2_PATCH
} TermWindow;

typedef struct {
	Display *dpy;
	Colormap cmap;
	Window win;
	Drawable buf;
	GlyphFontSpec *specbuf; /* font spec buffer used for rendering */
	Atom xembed, wmdeletewin, netwmname, netwmpid;
	struct {
		XIM xim;
		XIC xic;
		XPoint spot;
		XVaNestedList spotlist;
	} ime;
	Draw draw;
	Visual *vis;
	XSetWindowAttributes attrs;
	#if HIDECURSOR_PATCH
	/* Here, we use the term *pointer* to differentiate the cursor
	 * one sees when hovering the mouse over the terminal from, e.g.,
	 * a green rectangle where text would be entered. */
	Cursor vpointer, bpointer; /* visible and hidden pointers */
	int pointerisvisible;
	#endif // HIDECURSOR_PATCH
	int scr;
	int isfixed; /* is fixed geometry? */
	#if ALPHA_PATCH
	int depth; /* bit depth */
	#endif // ALPHA_PATCH
	int l, t; /* left and top offset */
	int gm; /* geometry mask */
} XWindow;

typedef struct {
	Atom xtarget;
	char *primary, *clipboard;
	struct timespec tclick1;
	struct timespec tclick2;
} XSelection;

/* Font structure */
#define Font Font_
typedef struct {
	int height;
	int width;
	int ascent;
	int descent;
	int badslant;
	int badweight;
	short lbearing;
	short rbearing;
	XftFont *match;
	FcFontSet *set;
	FcPattern *pattern;
} Font;

/* Drawing Context */
typedef struct {
	Color *col;
	size_t collen;
	Font font, bfont, ifont, ibfont;
	GC gc;
} DC;

static inline ushort sixd_to_16bit(int);
static int xmakeglyphfontspecs(XftGlyphFontSpec *, const Glyph *, int, int, int);
static void xdrawglyphfontspecs(const XftGlyphFontSpec *, Glyph, int, int, int);
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
static void xloadfonts(char *, double);
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
static void brelease(XEvent *);
static void bpress(XEvent *);
static void bmotion(XEvent *);
static void propnotify(XEvent *);
static void selnotify(XEvent *);
static void selclear_(XEvent *);
static void selrequest(XEvent *);
static void setsel(char *, Time);
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
static DC dc;
static XWindow xw;
static XSelection xsel;
static TermWindow win;

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

static int oldbutton = 3; /* button event on startup: 3 = release */
#if VISUALBELL_1_PATCH && !VISUALBELL_2_PATCH && !VISUALBELL_3_PATCH
static int bellon = 0;    /* visual bell status */
#endif // VISUALBELL_1_PATCH

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


int
mouseaction(XEvent *e, uint release)
{
	MouseShortcut *ms;

	#if SCROLLBACK_MOUSE_ALTSCREEN_PATCH
	if (tisaltscr())
		for (ms = maltshortcuts; ms < maltshortcuts + LEN(maltshortcuts); ms++) {
			if (ms->release == release &&
					ms->button == e->xbutton.button &&
					(match(ms->mod, e->xbutton.state) ||  /* exact or forced */
					match(ms->mod, e->xbutton.state & ~forcemousemod))) {
				ms->func(&(ms->arg));
				return 1;
			}
		}
	else
	#endif // SCROLLBACK_MOUSE_ALTSCREEN_PATCH
	for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
		if (ms->release == release &&
				ms->button == e->xbutton.button &&
				(match(ms->mod, e->xbutton.state) ||  /* exact or forced */
				match(ms->mod, e->xbutton.state & ~forcemousemod))) {
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
	int len, x = evcol(e), y = evrow(e),
	    button = e->xbutton.button, state = e->xbutton.state;
	char buf[40];
	static int ox, oy;

	/* from urxvt */
	if (e->xbutton.type == MotionNotify) {
		if (x == ox && y == oy)
			return;
		if (!IS_SET(MODE_MOUSEMOTION) && !IS_SET(MODE_MOUSEMANY))
			return;
		/* MOUSE_MOTION: no reporting if no button is pressed */
		if (IS_SET(MODE_MOUSEMOTION) && oldbutton == 3)
			return;

		button = oldbutton + 32;
		ox = x;
		oy = y;
	} else {
		if (!IS_SET(MODE_MOUSESGR) && e->xbutton.type == ButtonRelease) {
			button = 3;
		} else {
			button -= Button1;
			if (button >= 3)
				button += 64 - 3;
		}
		if (e->xbutton.type == ButtonPress) {
			oldbutton = button;
			ox = x;
			oy = y;
		} else if (e->xbutton.type == ButtonRelease) {
			oldbutton = 3;
			/* MODE_MOUSEX10: no button release reporting */
			if (IS_SET(MODE_MOUSEX10))
				return;
			if (button == 64 || button == 65)
				return;
		}
	}

	if (!IS_SET(MODE_MOUSEX10)) {
		button += ((state & ShiftMask  ) ? 4  : 0)
			+ ((state & Mod4Mask   ) ? 8  : 0)
			+ ((state & ControlMask) ? 16 : 0);
	}

	if (IS_SET(MODE_MOUSESGR)) {
		len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c",
				button, x+1, y+1,
				e->xbutton.type == ButtonRelease ? 'm' : 'M');
	} else if (x < 223 && y < 223) {
		len = snprintf(buf, sizeof(buf), "\033[M%c%c%c",
				32+button, 32+x+1, 32+y+1);
	} else {
		return;
	}

	ttywrite(buf, len, 0);
}

void
bpress(XEvent *e)
{
	struct timespec now;
	int snap;

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	if (mouseaction(e, 0))
		return;

	if (e->xbutton.button == Button1) {
		/*
		 * If the user clicks below predefined timeouts specific
		 * snapping behaviour is exposed.
		 */
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (TIMEDIFF(now, xsel.tclick2) <= tripleclicktimeout) {
			snap = SNAP_LINE;
		} else if (TIMEDIFF(now, xsel.tclick1) <= doubleclicktimeout) {
			snap = SNAP_WORD;
		} else {
			snap = 0;
		}
		xsel.tclick2 = xsel.tclick1;
		xsel.tclick1 = now;

		selstart(evcol(e), evrow(e), snap);
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

		if (e->type == PropertyNotify && nitems == 0 && rem == 0) {
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
			MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
					&xw.attrs);

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

void
xsetsel(char *str)
{
	setsel(str, CurrentTime);
}

void
brelease(XEvent *e)
{
	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	if (mouseaction(e, 1))
		return;
	if (e->xbutton.button == Button1)
		mousesel(e, 1);
	#if RIGHTCLICKTOPLUMB_PATCH
	else if (e->xbutton.button == Button3)
		plumb(xsel.primary);
	#endif // RIGHTCLICKTOPLUMB_PATCH
}

void
bmotion(XEvent *e)
{
	#if HIDECURSOR_PATCH
	if (!xw.pointerisvisible) {
		XDefineCursor(xw.dpy, xw.win, xw.vpointer);
		xw.pointerisvisible = 1;
		if (!IS_SET(MODE_MOUSEMANY))
			xsetpointermotion(0);
	}
	#endif // HIDECURSOR_PATCH

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

	XFreePixmap(xw.dpy, xw.buf);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			#if ALPHA_PATCH
			xw.depth
			#else
			DefaultDepth(xw.dpy, xw.scr)
			#endif // ALPHA_PATCH
	);
	XftDrawChange(xw.draw, xw.buf);
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

	return 0;
}

/*
 * Absolute coordinates.
 */
void
xclear(int x1, int y1, int x2, int y2)
{
	#if INVERT_PATCH
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
	#if ANYSIZE_PATCH
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

	match = FcFontMatch(NULL, configured, &result);
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
	f->width = DIVCEIL(extents.xOff, strlen(ascii_printable));

	return 0;
}

void
xloadfonts(char *fontstr, double fontsize)
{
	FcPattern *pattern;
	double fontval;

	if (fontstr[0] == '-')
		pattern = XftXlfdParse(fontstr, False, False);
	else
		pattern = FcNameParse((FcChar8 *)fontstr);

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
	#else
	Cursor cursor;
	#endif // HIDECURSOR_PATCH
	Window parent;
	pid_t thispid = getpid();
	XColor xmousefg, xmousebg;
	#if ALPHA_PATCH
	XWindowAttributes attr;
	XVisualInfo vis;
	#endif // ALPHA_PATCH

	if (!(xw.dpy = XOpenDisplay(NULL)))
		die("can't open display\n");
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
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h, xw.depth);
	dc.gc = XCreateGC(xw.dpy, xw.buf, GCGraphicsExposures, &gcvalues);
	#else
	dc.gc = XCreateGC(xw.dpy, parent, GCGraphicsExposures,
			&gcvalues);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			DefaultDepth(xw.dpy, xw.scr));
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

	xw.xembed = XInternAtom(xw.dpy, "_XEMBED", False);
	xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
	xw.netwmname = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
	XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);

	xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmpid, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)&thispid, 1);

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

	#if VERTCENTER_PATCH
	for (i = 0, xp = winx, yp = winy + font->ascent + win.cyo; i < len; ++i)
	#else
	for (i = 0, xp = winx, yp = winy + font->ascent; i < len; ++i)
	#endif // VERTCENTER_PATCH
	{
		/* Fetch rune and mode for current glyph. */
		rune = glyphs[i].u;
		mode = glyphs[i].mode;

		/* Skip dummy wide-character spacing. */
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
				font->set = FcFontSort(0, font->pattern,
				                       1, 0, &fcres);
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
			FcPatternAddCharSet(fcpattern, FC_CHARSET,
					fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

			FcConfigSubstitute(0, fcpattern,
					FcMatchPattern);
			FcDefaultSubstitute(fcpattern);

			fontpattern = FcFontSetMatch(0, fcsets, 1,
					fcpattern, &fcres);

			/* Allocate memory for the new cache entry. */
			if (frclen >= frccap) {
				frccap += 16;
				frc = xrealloc(frc, frccap * sizeof(Fontcache));
 			}

			frc[frclen].font = XftFontOpenPattern(xw.dpy,
					fontpattern);
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

	return numspecs;
}

void
xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Glyph base, int len, int x, int y)
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

	/* Intelligent cleaning up of the borders. */
	#if ANYSIZE_PATCH
	if (x == 0) {
		xclear(0, (y == 0)? 0 : winy, win.vborderpx,
			winy + win.ch +
			((winy + win.ch >= win.vborderpx + win.th)? win.h : 0));
	}
	if (winx + width >= win.hborderpx + win.tw) {
		xclear(winx + width, (y == 0)? 0 : winy, win.w,
			((winy + win.ch >= win.vborderpx + win.th)? win.h : (winy + win.ch)));
	}
	if (y == 0)
		xclear(winx, 0, winx + width, win.hborderpx);
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
	XftDrawRect(xw.draw, bg, winx, winy, width, win.ch);

	/* Set the clip region because Xft is sometimes dirty. */
	r.x = 0;
	r.y = 0;
	r.height = win.ch;
	r.width = width;
	XftDrawSetClipRectangles(xw.draw, winx, winy, &r, 1);

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
		#if VERTCENTER_PATCH
		XftDrawRect(xw.draw, fg, winx, winy + win.cyo + dc.font.ascent + 1,
				width, 1);
		#else
		XftDrawRect(xw.draw, fg, winx, winy + dc.font.ascent + 1,
				width, 1);
		#endif
	}

	if (base.mode & ATTR_STRUCK) {
		#if VERTCENTER_PATCH
		XftDrawRect(xw.draw, fg, winx, winy + win.cyo + 2 * dc.font.ascent / 3,
				width, 1);
		#else
		XftDrawRect(xw.draw, fg, winx, winy + 2 * dc.font.ascent / 3,
				width, 1);
		#endif // VERTCENTER_PATCH
	}

	/* Reset clip to none. */
	XftDrawSetClip(xw.draw, 0);
}

void
xdrawglyph(Glyph g, int x, int y)
{
	int numspecs;
	XftGlyphFontSpec spec;

	numspecs = xmakeglyphfontspecs(&spec, &g, 1, x, y);
	xdrawglyphfontspecs(&spec, g, numspecs, x, y);
}

void
xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
{
	Color drawcol;

	/* remove the old cursor */
	if (selected(ox, oy))
		og.mode ^= ATTR_REVERSE;
	xdrawglyph(og, ox, oy);

	if (IS_SET(MODE_HIDE))
		return;

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
		} else {
			g.fg = defaultbg;
			g.bg = defaultcs;
		}
		drawcol = dc.col[g.bg];
	}

	/* draw the new one */
	if (IS_SET(MODE_FOCUSED)) {
		switch (win.cursor) {
		case 7: /* st extension: snowman (U+2603) */
			g.u = 0x2603;
		case 0: /* Blinking Block */
		case 1: /* Blinking Block (Default) */
		case 2: /* Steady Block */
			xdrawglyph(g, cx, cy);
			break;
		case 3: /* Blinking Underline */
		case 4: /* Steady Underline */
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
xsettitle(char *p)
{
	XTextProperty prop;
	DEFAULT(p, opt_title);

	Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
			&prop);
	XSetWMName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
	XFree(prop.value);
}

int
xstartdraw(void)
{
	return IS_SET(MODE_VISIBLE);
}

void
xdrawline(Line line, int x1, int y1, int x2)
{
	int i, x, ox, numspecs;
	Glyph base, new;
	XftGlyphFontSpec *specs = xw.specbuf;

	numspecs = xmakeglyphfontspecs(specs, &line[x1], x2 - x1, x1, y1);
	i = ox = 0;
	for (x = x1; x < x2 && i < numspecs; x++) {
		new = line[x];
		if (new.mode == ATTR_WDUMMY)
			continue;
		if (selected(x, y1))
			new.mode ^= ATTR_REVERSE;
		#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
		if (win.vbellset && isvbellcell(x, y1))
			new.mode ^= ATTR_REVERSE;
		#endif // VISUALBELL_2_PATCH
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
}

void
xfinishdraw(void)
{
	#if VISUALBELL_3_PATCH
	if (vbellmode == 3 && win.vbellset)
		xdrawvbell();
	#endif // VISUALBELL_3_PATCH
	XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, win.w,
			win.h, 0, 0);
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
	MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
	XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
}

void
xsetmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
		redraw();
}

int
xsetcursor(int cursor)
{
	DEFAULT(cursor, 1);
	if (!BETWEEN(cursor, 0, 6))
		return 1;
	win.cursor = cursor;
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

	#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
	if (vbelltimeout)
		vbellbegin();
	#elif VISUALBELL_1_PATCH
	/* visual bell*/
	if (!bellon) {
		bellon = 1;
		MODBIT(win.mode, !IS_SET(MODE_REVERSE), MODE_REVERSE);
		redraw();
		XFlush(xw.dpy);
		MODBIT(win.mode, !IS_SET(MODE_REVERSE), MODE_REVERSE);
	}
	#endif // VISUALBELL_1_PATCH / VISUALBELL_2_PATCH
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
	} else {
		if (xw.ime.xic)
			XUnsetICFocus(xw.ime.xic);
		win.mode &= ~MODE_FOCUSED;
		if (IS_SET(MODE_FOCUS))
			ttywrite("\033[O", 3, 0);
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
	KeySym ksym;
	char buf[64], *customkey;
	int len;
	Rune c;
	Status status;
	Shortcut *bp;

	#if HIDECURSOR_PATCH
	if (xw.pointerisvisible) {
		XDefineCursor(xw.dpy, xw.win, xw.bpointer);
		xsetpointermotion(1);
		xw.pointerisvisible = 0;
	}
	#endif // HIDECURSOR_PATCH

	if (IS_SET(MODE_KBDLOCK))
		return;

	if (xw.ime.xic)
		len = XmbLookupString(xw.ime.xic, e, buf, sizeof buf, &ksym, &status);
	else
		len = XLookupString(e, buf, sizeof buf, &ksym, NULL);
	#if KEYBOARDSELECT_PATCH
	if ( IS_SET(MODE_KBDSELECT) ) {
		if ( match(XK_NO_MOD, e->state) ||
			(XK_Shift_L | XK_Shift_R) & e->state )
			win.mode ^= trt_kbdselect(ksym, buf, len);
		return;
	}
	#endif // KEYBOARDSELECT_PATCH

	/* 1. shortcuts */
	for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
		if (ksym == bp->keysym && match(bp->mod, e->state)) {
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
	int xfd = XConnectionNumber(xw.dpy), xev, blinkset = 0, dodraw = 0;
	int ttyfd;
	struct timespec drawtimeout, *tv = NULL, now, last, lastblink;
	long deltatime;
	#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
	long to_ms, remain;
	#endif // VISUALBELL_2_PATCH

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

	clock_gettime(CLOCK_MONOTONIC, &last);
	lastblink = last;

	for (xev = actionfps;;) {
		FD_ZERO(&rfd);
		FD_SET(ttyfd, &rfd);
		FD_SET(xfd, &rfd);

		if (pselect(MAX(xfd, ttyfd)+1, &rfd, NULL, NULL, tv, NULL) < 0) {
			if (errno == EINTR)
				continue;
			die("select failed: %s\n", strerror(errno));
		}
		if (FD_ISSET(ttyfd, &rfd)) {
			ttyread();
			if (blinktimeout) {
				blinkset = tattrset(ATTR_BLINK);
				if (!blinkset)
					MODBIT(win.mode, 0, MODE_BLINK);
			}
		}

		if (FD_ISSET(xfd, &rfd))
			xev = actionfps;

		clock_gettime(CLOCK_MONOTONIC, &now);
		drawtimeout.tv_sec = 0;
		drawtimeout.tv_nsec =  (1000 * 1E6)/ xfps;
		tv = &drawtimeout;

		dodraw = 0;
		#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
		to_ms = -1; /* timeout in ms, indefinite if negative */
		if (blinkset) {
			remain = blinktimeout - TIMEDIFF(now, lastblink);
			if (remain <= 0) {
				dodraw = 1;
				remain = 1; /* draw, wait 1ms, and re-calc */
				tsetdirtattr(ATTR_BLINK);
				win.mode ^= MODE_BLINK;
				lastblink = now;
			}
			to_ms = remain;
		}
		if (win.vbellset) {
			remain = vbelltimeout - TIMEDIFF(now, win.lastvbell);
			if (remain <= 0) {
				dodraw = 1;
				remain = -1; /* draw (clear), and that's it */
				tfulldirt();
				win.vbellset = 0;
			}
			if (remain >= 0 && (to_ms < 0 || remain < to_ms))
				to_ms = remain;
		}

		#else
		if (blinktimeout && TIMEDIFF(now, lastblink) > blinktimeout) {
			tsetdirtattr(ATTR_BLINK);
			win.mode ^= MODE_BLINK;
			lastblink = now;
			dodraw = 1;
		}
		#endif // VISUALBELL_2_PATCH
		deltatime = TIMEDIFF(now, last);
		if (deltatime > 1000 / (xev ? xfps : actionfps)) {
			dodraw = 1;
			last = now;
		}

		if (dodraw) {
			while (XPending(xw.dpy)) {
				XNextEvent(xw.dpy, &ev);
				if (XFilterEvent(&ev, None))
					continue;
				if (handler[ev.type])
					(handler[ev.type])(&ev);
			}

			#if VISUALBELL_1_PATCH && !VISUALBELL_2_PATCH && !VISUALBELL_3_PATCH
			if (bellon) {
				bellon = 0;
				redraw();
			}
			else draw();
 			XFlush(xw.dpy);
			#else
			draw();
			#endif // VISUALBELL_1_PATCH
			XFlush(xw.dpy);

			if (xev && !FD_ISSET(xfd, &rfd))
				xev--;
			if (!FD_ISSET(ttyfd, &rfd) && !FD_ISSET(xfd, &rfd)) {
				#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
				if (to_ms >= 0) {
					static const long k = 1E3, m = 1E6;
					drawtimeout.tv_sec = to_ms / k;
					drawtimeout.tv_nsec = (to_ms % k) * m;
				} else {
					tv = NULL;
				}
				#else
				if (blinkset) {
					if (TIMEDIFF(now, lastblink) \
							> blinktimeout) {
						drawtimeout.tv_nsec = 1000;
					} else {
						drawtimeout.tv_nsec = (1E6 * \
							(blinktimeout - \
							TIMEDIFF(now,
								lastblink)));
					}
					drawtimeout.tv_sec = \
					    drawtimeout.tv_nsec / 1E9;
					drawtimeout.tv_nsec %= (long)1E9;
				} else {
					tv = NULL;
				}
				#endif // VISUALBELL_2_PATCH
			}
		}
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
	win.cursor = cursorshape;

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
	#if XRESOURCES_PATCH
	if (!(xw.dpy = XOpenDisplay(NULL)))
		die("Can't open display\n");

	config_init();
	#endif // XRESOURCES_PATCH
	cols = MAX(cols, 1);
	rows = MAX(rows, 1);
	tnew(cols, rows);
	xinit(cols, rows);
	xsetenv();
	selinit();
	#if WORKINGDIR_PATCH
	if (opt_dir && chdir(opt_dir))
		die("Can't change to working directory %s\n", opt_dir);
	#endif // WORKINGDIR_PATCH
	run();

	return 0;
}
