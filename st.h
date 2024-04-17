/* See LICENSE for license details. */

#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include "patches.h"

/* macros */
#define MIN(a, b)		((a) < (b) ? (a) : (b))
#define MAX(a, b)		((a) < (b) ? (b) : (a))
#define LEN(a)			(sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)	((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)		(((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)		(a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)		(x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#if LIGATURES_PATCH
#define ATTRCMP(a, b)		(((a).mode & (~ATTR_WRAP) & (~ATTR_LIGA)) != ((b).mode & (~ATTR_WRAP) & (~ATTR_LIGA)) || \
				(a).fg != (b).fg || \
				(a).bg != (b).bg)
#else
#define ATTRCMP(a, b)		((a).mode != (b).mode || (a).fg != (b).fg || \
				(a).bg != (b).bg)
#endif // LIGATURES_PATCH
#define TIMEDIFF(t1, t2)	((t1.tv_sec-t2.tv_sec)*1000 + \
				(t1.tv_nsec-t2.tv_nsec)/1E6)
#define MODBIT(x, set, bit)	((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r,g,b)	(1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)		(1 << 24 & (x))
#if SCROLLBACK_PATCH || REFLOW_PATCH
#define HISTSIZE      2000
#endif // SCROLLBACK_PATCH | REFLOW_PATCH

enum glyph_attribute {
	ATTR_NULL           = 0,
	ATTR_SET            = 1 << 0,
	ATTR_BOLD           = 1 << 1,
	ATTR_FAINT          = 1 << 2,
	ATTR_ITALIC         = 1 << 3,
	ATTR_UNDERLINE      = 1 << 4,
	ATTR_BLINK          = 1 << 5,
	ATTR_REVERSE        = 1 << 6,
	ATTR_INVISIBLE      = 1 << 7,
	ATTR_STRUCK         = 1 << 8,
	ATTR_WRAP           = 1 << 9,
	ATTR_WIDE           = 1 << 10,
	ATTR_WDUMMY         = 1 << 11,
	#if SELECTION_COLORS_PATCH
	ATTR_SELECTED       = 1 << 12,
	#endif // SELECTION_COLORS_PATCH | REFLOW_PATCH
	#if BOXDRAW_PATCH
	ATTR_BOXDRAW        = 1 << 13,
	#endif // BOXDRAW_PATCH
	#if UNDERCURL_PATCH
	ATTR_DIRTYUNDERLINE = 1 << 14,
	#endif // UNDERCURL_PATCH
	#if LIGATURES_PATCH
	ATTR_LIGA           = 1 << 15,
	#endif // LIGATURES_PATCH
	#if SIXEL_PATCH
	ATTR_SIXEL          = 1 << 16,
	#endif // SIXEL_PATCH
	#if KEYBOARDSELECT_PATCH && REFLOW_PATCH
	ATTR_HIGHLIGHT      = 1 << 17,
	#endif // KEYBOARDSELECT_PATCH
	ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
};

#if SIXEL_PATCH
typedef struct _ImageList {
	struct _ImageList *next, *prev;
	unsigned char *pixels;
	void *pixmap;
	void *clipmask;
	int width;
	int height;
	int x;
	int y;
	#if REFLOW_PATCH
	int reflow_y;
	#endif // REFLOW_PATCH
	int cols;
	int cw;
	int ch;
	int transparent;
} ImageList;
#endif // SIXEL_PATCH

#if WIDE_GLYPHS_PATCH
enum drawing_mode {
	DRAW_NONE = 0,
	DRAW_BG   = 1 << 0,
	DRAW_FG   = 1 << 1,
};
#endif // WIDE_GLYPHS_PATCH

/* Used to control which screen(s) keybindings and mouse shortcuts apply to. */
enum screen {
	S_PRI = -1, /* primary screen */
	S_ALL = 0,  /* both primary and alt screen */
	S_ALT = 1   /* alternate screen */
};

enum selection_mode {
	SEL_IDLE = 0,
	SEL_EMPTY = 1,
	SEL_READY = 2
};

enum selection_type {
	SEL_REGULAR = 1,
	SEL_RECTANGULAR = 2
};

enum selection_snap {
	SNAP_WORD = 1,
	SNAP_LINE = 2
};

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef uint_least32_t Rune;

typedef XftDraw *Draw;
typedef XftColor Color;
typedef XftGlyphFontSpec GlyphFontSpec;

#define Glyph Glyph_
typedef struct {
	Rune u;           /* character code */
	uint32_t mode;    /* attribute flags */
	uint32_t fg;      /* foreground  */
	uint32_t bg;      /* background  */
	#if UNDERCURL_PATCH
	int ustyle;	      /* underline style */
	int ucolor[3];    /* underline color */
	#endif // UNDERCURL_PATCH
} Glyph;

typedef Glyph *Line;

#if LIGATURES_PATCH
typedef struct {
	int ox;
	int charlen;
	int numspecs;
	Glyph base;
} GlyphFontSeq;
#endif // LIGATURES_PATCH

typedef struct {
	Glyph attr; /* current char attributes */
	int x;
	int y;
	char state;
} TCursor;

/* Internal representation of the screen */
typedef struct {
	int row;      /* nb row */
	int col;      /* nb col */
	#if COLUMNS_PATCH
	int maxcol;
	#endif // COLUMNS_PATCH
	Line *line;   /* screen */
	Line *alt;    /* alternate screen */
	#if REFLOW_PATCH
	Line hist[HISTSIZE]; /* history buffer */
	int histi;           /* history index */
	int histf;           /* nb history available */
	int scr;             /* scroll back */
	int wrapcwidth[2];   /* used in updating WRAPNEXT when resizing */
	#elif SCROLLBACK_PATCH
	Line hist[HISTSIZE]; /* history buffer */
	int histi;    /* history index */
	int histn;    /* number of history entries */
	int scr;      /* scroll back */
	#endif // SCROLLBACK_PATCH | REFLOW_PATCH
	int *dirty;   /* dirtyness of lines */
	TCursor c;    /* cursor */
	int ocx;      /* old cursor col */
	int ocy;      /* old cursor row */
	int top;      /* top    scroll limit */
	int bot;      /* bottom scroll limit */
	int mode;     /* terminal mode flags */
	int esc;      /* escape state flags */
	char trantbl[4]; /* charset table translation */
	int charset;  /* current charset */
	int icharset; /* selected charset for sequence */
	int *tabs;
	#if SIXEL_PATCH
	ImageList *images;     /* sixel images */
	ImageList *images_alt; /* sixel images for alternate screen */
	#endif // SIXEL_PATCH
	Rune lastc;   /* last printed char outside of sequence, 0 if control */
} Term;

typedef union {
	int i;
	uint ui;
	float f;
	const void *v;
	const char *s;
} Arg;

/* Purely graphic info */
typedef struct {
	int tw, th; /* tty width and height */
	int w, h; /* window width and height */
	#if BACKGROUND_IMAGE_PATCH
	int x, y; /* window location */
	#endif // BACKGROUND_IMAGE_PATCH
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
} TermWindow;

typedef struct {
	Display *dpy;
	Colormap cmap;
	Window win;
	Drawable buf;
	GlyphFontSpec *specbuf; /* font spec buffer used for rendering */
	#if LIGATURES_PATCH
	GlyphFontSeq *specseq;
	#endif // LIGATURES_PATCH
	Atom xembed, wmdeletewin, netwmname, netwmiconname, netwmpid;
	#if FULLSCREEN_PATCH
	Atom netwmstate, netwmfullscreen;
	#endif // FULLSCREEN_PATCH
	#if NETWMICON_PATCH || NETWMICON_LEGACY_PATCH || NETWMICON_FF_PATCH
	Atom netwmicon;
	#endif // NETWMICON_PATCH
	struct {
		XIM xim;
		XIC xic;
		XPoint spot;
		XVaNestedList spotlist;
	} ime;
	Draw draw;
	#if BACKGROUND_IMAGE_PATCH
	GC bggc;          /* Graphics Context for background */
	#endif // BACKGROUND_IMAGE_PATCH
	Visual *vis;
	XSetWindowAttributes attrs;
	#if HIDECURSOR_PATCH || OPENURLONCLICK_PATCH
	/* Here, we use the term *pointer* to differentiate the cursor
	 * one sees when hovering the mouse over the terminal from, e.g.,
	 * a green rectangle where text would be entered. */
	Cursor vpointer, bpointer; /* visible and hidden pointers */
	int pointerisvisible;
	#endif // HIDECURSOR_PATCH
	#if OPENURLONCLICK_PATCH
	Cursor upointer;
	#endif // OPENURLONCLICK_PATCH
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

/* types used in config.h */
typedef struct {
	uint mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
	int screen;
} Shortcut;

typedef struct {
	uint mod;
	uint button;
	void (*func)(const Arg *);
	const Arg arg;
	uint release;
	int screen;
} MouseShortcut;

typedef struct {
	KeySym k;
	uint mask;
	char *s;
	/* three-valued logic variables: 0 indifferent, 1 on, -1 off */
	signed char appkey;    /* application keypad */
	signed char appcursor; /* application cursor */
} Key;

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

void die(const char *, ...);
void redraw(void);
void draw(void);
void drawregion(int, int, int, int);
void tfulldirt(void);

void printscreen(const Arg *);
void printsel(const Arg *);
void sendbreak(const Arg *);
void toggleprinter(const Arg *);

int tattrset(int);
int tisaltscr(void);
void tnew(int, int);
void tresize(int, int);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);

void resettitle(void);

void selclear(void);
void selinit(void);
void selremove(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
int selected(int, int);
char *getsel(void);

size_t utf8encode(Rune, char *);

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b);

#if BOXDRAW_PATCH
int isboxdraw(Rune);
ushort boxdrawindex(const Glyph *);
#ifdef XFT_VERSION
/* only exposed to x.c, otherwise we'll need Xft.h for the types */
void boxdraw_xinit(Display *, Colormap, XftDraw *, Visual *);
void drawboxes(int, int, int, int, XftColor *, XftColor *, const XftGlyphFontSpec *, int);
#endif // XFT_VERSION
#endif // BOXDRAW_PATCH

/* config.h globals */
extern char *utmp;
extern char *scroll;
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
#if KEYBOARDSELECT_PATCH && REFLOW_PATCH
extern wchar_t *kbds_sdelim;
extern wchar_t *kbds_ldelim;
#endif // KEYBOARDSELECT_PATCH
extern int allowaltscreen;
extern int allowwindowops;
extern char *termname;
extern unsigned int tabspaces;
extern unsigned int defaultfg;
extern unsigned int defaultbg;
extern unsigned int defaultcs;
#if EXTERNALPIPE_PATCH
extern int extpipeactive;
#endif // EXTERNALPIPE_PATCH

#if BOXDRAW_PATCH
extern const int boxdraw, boxdraw_bold, boxdraw_braille;
#endif // BOXDRAW_PATCH
#if ALPHA_PATCH
extern float alpha;
#if ALPHA_FOCUS_HIGHLIGHT_PATCH
extern float alphaUnfocused;
#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH
#endif // ALPHA_PATCH

extern DC dc;
extern XWindow xw;
extern XSelection xsel;
extern TermWindow win;
extern Term term;
