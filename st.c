/* See LICENSE for license details. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

#include "st.h"
#include "win.h"

#if VIM_BROWSE_PATCH
#include <assert.h>
#endif // VIM_BROWSE_PATCH

#if KEYBOARDSELECT_PATCH
#include <X11/keysym.h>
#include <X11/X.h>
#endif // KEYBOARDSELECT_PATCH

#if SIXEL_PATCH
#include "sixel.h"
#endif // SIXEL_PATCH

#if   defined(__linux)
 #include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
 #include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
 #include <libutil.h>
#endif

/* Arbitrary sizes */
#define UTF_INVALID   0xFFFD
#define UTF_SIZ       4
#define ESC_BUF_SIZ   (128*UTF_SIZ)
#define ESC_ARG_SIZ   16
#if UNDERCURL_PATCH
#define CAR_PER_ARG   4
#endif // UNDERCURL_PATCH
#define STR_BUF_SIZ   ESC_BUF_SIZ
#define STR_ARG_SIZ   ESC_ARG_SIZ

/* macros */
#define IS_SET(flag)    ((term.mode & (flag)) != 0)
#define ISCONTROLC0(c)  (BETWEEN(c, 0, 0x1f) || (c) == 0x7f)
#define ISCONTROLC1(c)  (BETWEEN(c, 0x80, 0x9f))
#define ISCONTROL(c)    (ISCONTROLC0(c) || ISCONTROLC1(c))
#define ISDELIM(u)      (u && wcschr(worddelimiters, u))
#if VIM_BROWSE_PATCH
static inline int max(int a, int b) { return a > b ? a : b; }
static inline int min(int a, int b) { return a < b ? a : b; }
#endif // VIM_BROWSE_PATCH

enum term_mode {
	MODE_WRAP        = 1 << 0,
	MODE_INSERT      = 1 << 1,
	MODE_ALTSCREEN   = 1 << 2,
	MODE_CRLF        = 1 << 3,
	MODE_ECHO        = 1 << 4,
	MODE_PRINT       = 1 << 5,
	MODE_UTF8        = 1 << 6,
	#if SIXEL_PATCH
	MODE_SIXEL       = 1 << 7,
	#endif // SIXEL_PATCH
};

enum cursor_movement {
	CURSOR_SAVE,
	CURSOR_LOAD
};

enum cursor_state {
	CURSOR_DEFAULT  = 0,
	CURSOR_WRAPNEXT = 1,
	CURSOR_ORIGIN   = 2
};

enum charset {
	CS_GRAPHIC0,
	CS_GRAPHIC1,
	CS_UK,
	CS_USA,
	CS_MULTI,
	CS_GER,
	CS_FIN
};

enum escape_state {
	ESC_START      = 1,
	ESC_CSI        = 2,
	ESC_STR        = 4,  /* DCS, OSC, PM, APC */
	ESC_ALTCHARSET = 8,
	ESC_STR_END    = 16, /* a final string was encountered */
	ESC_TEST       = 32, /* Enter in test mode */
	ESC_UTF8       = 64,
	#if SIXEL_PATCH
	ESC_DCS        =128,
	#endif // SIXEL_PATCH
};

typedef struct {
	int mode;
	int type;
	int snap;
	#if VIM_BROWSE_PATCH
	int swap;
	#endif // VIM_BROWSE_PATCH
	/*
	 * Selection variables:
	 * nb – normalized coordinates of the beginning of the selection
	 * ne – normalized coordinates of the end of the selection
	 * ob – original coordinates of the beginning of the selection
	 * oe – original coordinates of the end of the selection
	 */
	struct {
		int x, y;
	} nb, ne, ob, oe;

	int alt;
} Selection;

/* CSI Escape sequence structs */
/* ESC '[' [[ [<priv>] <arg> [;]] <mode> [<mode>]] */
typedef struct {
	char buf[ESC_BUF_SIZ]; /* raw string */
	size_t len;            /* raw string length */
	char priv;
	int arg[ESC_ARG_SIZ];
	int narg;              /* nb of args */
	char mode[2];
	#if UNDERCURL_PATCH
	int carg[ESC_ARG_SIZ][CAR_PER_ARG]; /* colon args */
	#endif // UNDERCURL_PATCH
} CSIEscape;

/* STR Escape sequence structs */
/* ESC type [[ [<priv>] <arg> [;]] <mode>] ESC '\' */
typedef struct {
	char type;             /* ESC type ... */
	char *buf;             /* allocated raw string */
	size_t siz;            /* allocation size */
	size_t len;            /* raw string length */
	char *args[STR_ARG_SIZ];
	int narg;              /* nb of args */
} STREscape;

static void execsh(char *, char **);
static void stty(char **);
static void sigchld(int);
static void ttywriteraw(const char *, size_t);

static void csidump(void);
static void csihandle(void);
#if UNDERCURL_PATCH
static void readcolonargs(char **, int, int[][CAR_PER_ARG]);
#endif // UNDERCURL_PATCH
static void csiparse(void);
static void csireset(void);
static int eschandle(uchar);
static void strdump(void);
static void strhandle(void);
static void strparse(void);
static void strreset(void);

static void tprinter(char *, size_t);
static void tdumpsel(void);
static void tdumpline(int);
static void tdump(void);
static void tclearregion(int, int, int, int);
static void tcursor(int);
static void tdeletechar(int);
static void tdeleteline(int);
static void tinsertblank(int);
static void tinsertblankline(int);
static int tlinelen(int);
#if !VIM_BROWSE_PATCH
static void tmoveto(int, int);
#endif // VIM_BROWSE_PATCH
static void tmoveato(int, int);
static void tnewline(int);
static void tputtab(int);
static void tputc(Rune);
static void treset(void);
#if SCROLLBACK_PATCH
static void tscrollup(int, int, int);
static void tscrolldown(int, int, int);
#else
static void tscrollup(int, int);
static void tscrolldown(int, int);
#endif // SCROLLBACK_PATCH
static void tsetattr(const int *, int);
static void tsetchar(Rune, const Glyph *, int, int);
static void tsetdirt(int, int);
static void tsetscroll(int, int);
static void tswapscreen(void);
static void tsetmode(int, int, const int *, int);
static int twrite(const char *, int, int);
static void tcontrolcode(uchar );
static void tdectest(char );
static void tdefutf8(char);
static int32_t tdefcolor(const int *, int *, int);
static void tdeftran(char);
static void tstrsequence(uchar);
static void selnormalize(void);
static void selscroll(int, int);
#if !VIM_BROWSE_PATCH
static void selsnap(int *, int *, int);
#endif // VIM_BROWSE_PATCH

static size_t utf8decode(const char *, Rune *, size_t);
static Rune utf8decodebyte(char, size_t *);
static char utf8encodebyte(Rune, size_t);
static size_t utf8validate(Rune *, size_t);

static char *base64dec(const char *);
static char base64dec_getc(const char **);

static ssize_t xwrite(int, const char *, size_t);

/* Globals */
static Selection sel;
static CSIEscape csiescseq;
static STREscape strescseq;
static int iofd = 1;
static int cmdfd;
#if EXTERNALPIPEIN_PATCH && EXTERNALPIPE_PATCH
static int csdfd;
#endif // EXTERNALPIPEIN_PATCH
static pid_t pid;
#if SIXEL_PATCH
sixel_state_t sixel_st;
#endif // SIXEL_PATCH

static const uchar utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
static const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const Rune utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
static const Rune utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

#if VIM_BROWSE_PATCH
int buffCols;
extern int const buffSize;
int histOp, histMode, histOff, insertOff, altToggle, *mark;
Line *buf = NULL;
static TCursor c[3];
static inline int rows() { return IS_SET(MODE_ALTSCREEN) ? term.row : buffSize;}
static inline int rangeY(int i) { while (i < 0) i += rows(); return i % rows();}
#endif // VIM_BROWSE_PATCH

#include "patch/st_include.h"

ssize_t
xwrite(int fd, const char *s, size_t len)
{
	size_t aux = len;
	ssize_t r;

	while (len > 0) {
		r = write(fd, s, len);
		if (r < 0)
			return r;
		len -= r;
		s += r;
	}

	return aux;
}

void *
xmalloc(size_t len)
{
	void *p;

	if (!(p = malloc(len)))
		die("malloc: %s\n", strerror(errno));

	return p;
}

void *
xrealloc(void *p, size_t len)
{
	if ((p = realloc(p, len)) == NULL)
		die("realloc: %s\n", strerror(errno));

	return p;
}

char *
xstrdup(const char *s)
{
	char *p;
	if ((p = strdup(s)) == NULL)
		die("strdup: %s\n", strerror(errno));

	return p;
}

size_t
utf8decode(const char *c, Rune *u, size_t clen)
{
	size_t i, j, len, type;
	Rune udecoded;

	*u = UTF_INVALID;
	if (!clen)
		return 0;
	udecoded = utf8decodebyte(c[0], &len);
	if (!BETWEEN(len, 1, UTF_SIZ))
		return 1;
	for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if (type != 0)
			return j;
	}
	if (j < len)
		return 0;
	*u = udecoded;
	utf8validate(u, len);

	return len;
}

Rune
utf8decodebyte(char c, size_t *i)
{
	for (*i = 0; *i < LEN(utfmask); ++(*i))
		if (((uchar)c & utfmask[*i]) == utfbyte[*i])
			return (uchar)c & ~utfmask[*i];

	return 0;
}

size_t
utf8encode(Rune u, char *c)
{
	size_t len, i;

	len = utf8validate(&u, 0);
	if (len > UTF_SIZ)
		return 0;

	for (i = len - 1; i != 0; --i) {
		c[i] = utf8encodebyte(u, 0);
		u >>= 6;
	}
	c[0] = utf8encodebyte(u, len);

	return len;
}

char
utf8encodebyte(Rune u, size_t i)
{
	return utfbyte[i] | (u & ~utfmask[i]);
}

size_t
utf8validate(Rune *u, size_t i)
{
	if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
		*u = UTF_INVALID;
	for (i = 1; *u > utfmax[i]; ++i)
		;

	return i;
}

static const char base64_digits[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0,
	63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, -1, 0, 0, 0, 0, 1,
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
	22, 23, 24, 25, 0, 0, 0, 0, 0, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

char
base64dec_getc(const char **src)
{
	while (**src && !isprint(**src))
		(*src)++;
	return **src ? *((*src)++) : '=';  /* emulate padding if string ends */
}

char *
base64dec(const char *src)
{
	size_t in_len = strlen(src);
	char *result, *dst;

	if (in_len % 4)
		in_len += 4 - (in_len % 4);
	result = dst = xmalloc(in_len / 4 * 3 + 1);
	while (*src) {
		int a = base64_digits[(unsigned char) base64dec_getc(&src)];
		int b = base64_digits[(unsigned char) base64dec_getc(&src)];
		int c = base64_digits[(unsigned char) base64dec_getc(&src)];
		int d = base64_digits[(unsigned char) base64dec_getc(&src)];

		/* invalid input. 'a' can be -1, e.g. if src is "\n" (c-str) */
		if (a == -1 || b == -1)
			break;

		*dst++ = (a << 2) | ((b & 0x30) >> 4);
		if (c == -1)
			break;
		*dst++ = ((b & 0x0f) << 4) | ((c & 0x3c) >> 2);
		if (d == -1)
			break;
		*dst++ = ((c & 0x03) << 6) | d;
	}
	*dst = '\0';
	return result;
}

void
selinit(void)
{
	sel.mode = SEL_IDLE;
	sel.snap = 0;
	sel.ob.x = -1;
}

int
tlinelen(int y)
{
	int i = term.col;

	#if SCROLLBACK_PATCH
	if (TLINE(y)[i - 1].mode & ATTR_WRAP)
		return i;

	while (i > 0 && TLINE(y)[i - 1].u == ' ')
 		--i;
	#else
	if (term.line[y][i - 1].mode & ATTR_WRAP)
		return i;

	while (i > 0 && term.line[y][i - 1].u == ' ')
		--i;
	#endif // SCROLLBACK_PATCH

	return i;
}

#if VIM_BROWSE_PATCH
void historyOpToggle(int start, int paint) {
	if ((!histOp == !(histOp + start)) && ((histOp += start) || 1)) return;
	if (histMode && paint && (!IS_SET(MODE_ALTSCREEN) || altToggle)) draw();
	tcursor(CURSOR_SAVE);
	histOp += start;
	if (histMode && altToggle) {
		tswapscreen();
		memset(term.dirty,0,sizeof(*term.dirty)*term.row);
	}
	tcursor(CURSOR_LOAD);
	*(!IS_SET(MODE_ALTSCREEN)?&term.line:&term.alt)=&buf[histOp?histOff:insertOff];
}

void historyModeToggle(int start) {
	if (!(histMode = (histOp = !!start))) {
		selnormalize();
		tfulldirt();
	} else {
		tcursor(CURSOR_SAVE);
		histOp = 0;
		histOff = insertOff;
	}
}

int historyBufferScroll(int n) {
	if (IS_SET(MODE_ALTSCREEN) || !n) return histOp;
	int p=abs(n=(n<0) ? max(n,-term.row) : min(n,term.row)), r=term.row-p,
	          s=sizeof(*term.dirty), *ptr=histOp?&histOff:&insertOff;
	if (!histMode || histOp) tfulldirt(); else {
		memmove(&term.dirty[-min(n,0)], &term.dirty[max(n,0)], s*r);
		memset(&term.dirty[n>0 ? r : 0], 0, s * p);
	}
	term.line = &buf[*ptr = (buffSize+*ptr+n) % buffSize];
	// Cut part of selection removed from buffer, and update sel.ne/b.
	int const prevOffBuf = sel.alt ? 0 : insertOff + term.row;
	if (sel.ob.x != -1 && !histOp && n) {
		int const offBuf = sel.alt ? 0 : insertOff + term.row,
		          pb = rangeY(sel.ob.y - prevOffBuf),
		          pe = rangeY(sel.oe.y - prevOffBuf);
		int const b = rangeY(sel.ob.y - offBuf), nln = n < 0,
		          e = rangeY(sel.oe.y - offBuf), last = offBuf - nln;
		if (pb != b && ((pb < b) != nln)) sel.ob.y = last;
		if (pe != e && ((pe < e) != nln)) sel.oe.y = last;
		if (sel.oe.y == last && sel.ob.y == last) selclear();
	}
	selnormalize();
  // Clear the new region exposed by the shift.
	if (!histOp) tclearregion(0, n>0?r+1:0, buffCols-1, n>0?term.row:p-1);
	return 1;
}

int historyMove(int x, int y, int ly) {
	historyOpToggle(1, 1);
	y += ((term.c.x += x) < 0 ?term.c.x-term.col :term.c.x) / term.col;//< x
	if ((term.c.x %= term.col) < 0) term.c.x += term.col;
	if ((term.c.y += y) >= term.row) ly += term.c.y - term.row + 1;    //< y
	else if (term.c.y < 0) ly += term.c.y;
	term.c.y = MIN(MAX(term.c.y, 0), term.row - 1);
	// Check if scroll is necessary / arrived at top / bottom of terminal history
	int t = 0, b = 0, finTop = ly < 0, finBot = ly > 0;
	if (!IS_SET(MODE_ALTSCREEN)) {
		b=rangeY(insertOff-histOff), t=-rangeY(-term.row-(insertOff-histOff));
		finBot = ly > b, finTop=histMode&&((-ly>-t));
	}
	if ((finTop || finBot) && (x||y)) term.c.x = finBot ? term.col-1 : 0;
	historyBufferScroll(finBot ? b : (finTop ? t : ly));
	historyOpToggle(-1, 1);
	return finTop || finBot;
}

void selnormalize(void) {
	historyOpToggle(1, 1);

	int const oldb = sel.nb.y, olde = sel.ne.y;
	if (sel.ob.x == -1) {
		sel.ne.y = sel.nb.y = -1;
	} else {
		int const offsetBuffer = sel.alt ? 0 : insertOff + term.row;
		int const off = sel.alt ? 0 : (histMode ? histOff : insertOff);
		int const nby = rangeY(sel.ob.y - off),
		          ney = rangeY(sel.oe.y - off);
		sel.swap = rangeY(sel.ob.y - offsetBuffer)
		         > rangeY(sel.oe.y - offsetBuffer);
		sel.nb.y = sel.swap ? ney : nby;
		sel.ne.y = !sel.swap ? ney : nby;
		int const cnb = sel.nb.y < term.row, cne = sel.ne.y < term.row;
		if (sel.type == SEL_REGULAR && sel.ob.y != sel.oe.y) {
			if (cnb) sel.nb.x = (!sel.swap) ? sel.ob.x : sel.oe.x;
			if (cne) sel.ne.x = (!sel.swap) ? sel.oe.x : sel.ob.x;
		} else {
			if (cnb) sel.nb.x = MIN(sel.ob.x, sel.oe.x);
			if (cne) sel.ne.x = MAX(sel.ob.x, sel.oe.x);
		}
	}
	int const nBet=sel.nb.y<=sel.ne.y, oBet=oldb<=olde;
	for (int i = 0; i < term.row; ++i) {
		int const n = nBet ? BETWEEN(i, sel.nb.y, sel.ne.y)
		                   : OUT(i, sel.nb.y, sel.ne.y);
		term.dirty[i] |= (sel.type == SEL_RECTANGULAR && n) ||
		        (n != (oBet ? BETWEEN(i,oldb,olde) : OUT(i,oldb,olde)));

	}
	if (BETWEEN(oldb, 0, term.row - 1)) term.dirty[oldb] = 1;
	if (BETWEEN(olde, 0, term.row - 1)) term.dirty[olde] = 1;
	if (BETWEEN(sel.nb.y, 0, term.row - 1)) term.dirty[sel.nb.y] = 1;
	if (BETWEEN(sel.ne.y, 0, term.row - 1)) term.dirty[sel.ne.y] = 1;

	historyOpToggle(-1, 1);
}
#endif // VIM_BROWSE_PATCH

void
selstart(int col, int row, int snap)
{
	selclear();
	sel.mode = SEL_EMPTY;
	sel.type = SEL_REGULAR;
	sel.alt = IS_SET(MODE_ALTSCREEN);
	sel.snap = snap;
	sel.oe.x = sel.ob.x = col;
	#if VIM_BROWSE_PATCH
	sel.oe.y = sel.ob.y = row + !sel.alt * (histMode ? histOff : insertOff);
	if (sel.snap != 0) sel.mode = SEL_READY;
	#else
	sel.oe.y = sel.ob.y = row;
	#endif // VIM_BROWSE_PATCH
	selnormalize();

	#if !VIM_BROWSE_PATCH
	if (sel.snap != 0)
		sel.mode = SEL_READY;
	tsetdirt(sel.nb.y, sel.ne.y);
	#endif // VIM_BROWSE_PATCH
}

void
selextend(int col, int row, int type, int done)
{
	#if !VIM_BROWSE_PATCH
	int oldey, oldex, oldsby, oldsey, oldtype;
	#endif // VIM_BROWSE_PATCH

	if (sel.mode == SEL_IDLE)
		return;
	if (done && sel.mode == SEL_EMPTY) {
		selclear();
		return;
	}

	#if !VIM_BROWSE_PATCH
	oldey = sel.oe.y;
	oldex = sel.oe.x;
	oldsby = sel.nb.y;
	oldsey = sel.ne.y;
	oldtype = sel.type;
	#endif // VIM_BROWSE_PATCH

	sel.oe.x = col;
	#if VIM_BROWSE_PATCH
	sel.oe.y = row + (sel.alt ? 0 : (histMode ? histOff : insertOff));
	#else
	sel.oe.y = row;
	#endif // VIM_BROWSE_PATCH
	selnormalize();
	sel.type = type;

	#if !VIM_BROWSE_PATCH
	if (oldey != sel.oe.y || oldex != sel.oe.x || oldtype != sel.type || sel.mode == SEL_EMPTY)
		tsetdirt(MIN(sel.nb.y, oldsby), MAX(sel.ne.y, oldsey));
	#endif // VIM_BROWSE_PATCH

	sel.mode = done ? SEL_IDLE : SEL_READY;
}

#if !VIM_BROWSE_PATCH
void
selnormalize(void)
{
	int i;

	if (sel.type == SEL_REGULAR && sel.ob.y != sel.oe.y) {
		sel.nb.x = sel.ob.y < sel.oe.y ? sel.ob.x : sel.oe.x;
		sel.ne.x = sel.ob.y < sel.oe.y ? sel.oe.x : sel.ob.x;
	} else {
		sel.nb.x = MIN(sel.ob.x, sel.oe.x);
		sel.ne.x = MAX(sel.ob.x, sel.oe.x);
	}
	sel.nb.y = MIN(sel.ob.y, sel.oe.y);
	sel.ne.y = MAX(sel.ob.y, sel.oe.y);

	selsnap(&sel.nb.x, &sel.nb.y, -1);
	selsnap(&sel.ne.x, &sel.ne.y, +1);

	/* expand selection over line breaks */
	if (sel.type == SEL_RECTANGULAR)
		return;
	i = tlinelen(sel.nb.y);
	if (i < sel.nb.x)
		sel.nb.x = i;
	if (tlinelen(sel.ne.y) <= sel.ne.x)
		sel.ne.x = term.col - 1;
}
#endif // VIM_BROWSE_PATCH

int
selected(int x, int y)
{
	if (sel.mode == SEL_EMPTY || sel.ob.x == -1 ||
			sel.alt != IS_SET(MODE_ALTSCREEN))
		return 0;

	if (sel.type == SEL_RECTANGULAR)
		return BETWEEN(y, sel.nb.y, sel.ne.y)
		    && BETWEEN(x, sel.nb.x, sel.ne.x);

	#if VIM_BROWSE_PATCH
	return ((sel.nb.y > sel.ne.y) ? OUT(y, sel.nb.y, sel.ne.y)
	                              : BETWEEN(y, sel.nb.y, sel.ne.y)) &&
	       (y != sel.nb.y || x >= sel.nb.x) &&
	       (y != sel.ne.y || x <= sel.ne.x);
	#else
	return BETWEEN(y, sel.nb.y, sel.ne.y)
	    && (y != sel.nb.y || x >= sel.nb.x)
	    && (y != sel.ne.y || x <= sel.ne.x);
	#endif // VIM_BROWSE_PATCH
}

#if !VIM_BROWSE_PATCH
void
selsnap(int *x, int *y, int direction)
{
	int newx, newy, xt, yt;
	int delim, prevdelim;
	const Glyph *gp, *prevgp;

	switch (sel.snap) {
	case SNAP_WORD:
		/*
		 * Snap around if the word wraps around at the end or
		 * beginning of a line.
		 */
		#if SCROLLBACK_PATCH
		prevgp = &TLINE(*y)[*x];
		#else
		prevgp = &term.line[*y][*x];
		#endif // SCROLLBACK_PATCH
		prevdelim = ISDELIM(prevgp->u);
		for (;;) {
			newx = *x + direction;
			newy = *y;
			if (!BETWEEN(newx, 0, term.col - 1)) {
				newy += direction;
				newx = (newx + term.col) % term.col;
				if (!BETWEEN(newy, 0, term.row - 1))
					break;

				if (direction > 0)
					yt = *y, xt = *x;
				else
					yt = newy, xt = newx;
				#if SCROLLBACK_PATCH
				if (!(TLINE(yt)[xt].mode & ATTR_WRAP))
				#else
				if (!(term.line[yt][xt].mode & ATTR_WRAP))
				#endif // SCROLLBACK_PATCH
					break;
			}

			if (newx >= tlinelen(newy))
				break;

			#if SCROLLBACK_PATCH
			gp = &TLINE(newy)[newx];
			#else
			gp = &term.line[newy][newx];
			#endif // SCROLLBACK_PATCH
			delim = ISDELIM(gp->u);
			if (!(gp->mode & ATTR_WDUMMY) && (delim != prevdelim
					|| (delim && gp->u != prevgp->u)))
				break;

			*x = newx;
			*y = newy;
			prevgp = gp;
			prevdelim = delim;
		}
		break;
	case SNAP_LINE:
		/*
		 * Snap around if the the previous line or the current one
		 * has set ATTR_WRAP at its end. Then the whole next or
		 * previous line will be selected.
		 */
		*x = (direction < 0) ? 0 : term.col - 1;
		if (direction < 0) {
			for (; *y > 0; *y += direction) {
				#if SCROLLBACK_PATCH
				if (!(TLINE(*y-1)[term.col-1].mode & ATTR_WRAP))
				#else
				if (!(term.line[*y-1][term.col-1].mode & ATTR_WRAP))
				#endif // SCROLLBACK_PATCH
				{
					break;
				}
			}
		} else if (direction > 0) {
			for (; *y < term.row-1; *y += direction) {
				#if SCROLLBACK_PATCH
				if (!(TLINE(*y)[term.col-1].mode & ATTR_WRAP))
				#else
				if (!(term.line[*y][term.col-1].mode & ATTR_WRAP))
				#endif // SCROLLBACK_PATCH
				{
					break;
				}
			}
		}
		break;
	}
}
#endif // VIM_BROWSE_PATCH

char *
getsel(void)
{
	char *str, *ptr;
	#if VIM_BROWSE_PATCH
	int y, yy, bufsize, lastx;
	#else
	int y, bufsize, lastx, linelen;
	#endif // VIM_BROWSE_PATCH
	const Glyph *gp, *last;

	if (sel.ob.x == -1)
		return NULL;

	#if VIM_BROWSE_PATCH
	int const start = sel.swap ? sel.oe.y : sel.ob.y, h = rows();
	int endy = (sel.swap ? sel.ob.y : sel.oe.y);
	for (; endy < start; endy += h);
	Line * const cbuf = IS_SET(MODE_ALTSCREEN) ? term.line : buf;
	bufsize = (term.col+1) * (endy-start+1 ) * UTF_SIZ;
	assert(bufsize > 0);
	#else
	bufsize = (term.col+1) * (sel.ne.y-sel.nb.y+1) * UTF_SIZ;
	#endif // VIM_BROWSE_PATCH
	ptr = str = xmalloc(bufsize);

	/* append every set & selected glyph to the selection */
	#if VIM_BROWSE_PATCH
	for (y = start; y <= endy; y++)
	#else
	for (y = sel.nb.y; y <= sel.ne.y; y++)
	#endif // VIM_BROWSE_PATCH
	{
		#if VIM_BROWSE_PATCH
		yy = y % h;
		#else
		if ((linelen = tlinelen(y)) == 0) {
			*ptr++ = '\n';
			continue;
		}
		#endif // VIM_BROWSE_PATCH

		if (sel.type == SEL_RECTANGULAR) {
			#if VIM_BROWSE_PATCH
			gp = &cbuf[yy][sel.nb.x];
			#elif SCROLLBACK_PATCH
			gp = &TLINE(y)[sel.nb.x];
			#else
			gp = &term.line[y][sel.nb.x];
			#endif // SCROLLBACK_PATCH
			lastx = sel.ne.x;
		} else {
			#if VIM_BROWSE_PATCH
			gp = &cbuf[yy][start == y ? sel.nb.x : 0];
			#elif SCROLLBACK_PATCH
			gp = &TLINE(y)[sel.nb.y == y ? sel.nb.x : 0];
			#else
			gp = &term.line[y][sel.nb.y == y ? sel.nb.x : 0];
			#endif // SCROLLBACK_PATCH
			#if VIM_BROWSE_PATCH
			lastx = (endy == y) ? sel.ne.x : term.col-1;
			#else
			lastx = (sel.ne.y == y) ? sel.ne.x : term.col-1;
			#endif // VIM_BROWSE_PATCH
		}
		#if VIM_BROWSE_PATCH
		last = &cbuf[yy][lastx];
		#elif SCROLLBACK_PATCH
		last = &TLINE(y)[MIN(lastx, linelen-1)];
		#else
		last = &term.line[y][MIN(lastx, linelen-1)];
		#endif // SCROLLBACK_PATCH
		#if VIM_BROWSE_PATCH
		if (!(cbuf[yy][term.col - 1].mode & ATTR_WRAP))
			while (last > gp && last->u == ' ') --last;
		#else
		while (last >= gp && last->u == ' ')
			--last;
		#endif // VIM_BROWSE_PATCH

		for ( ; gp <= last; ++gp) {
			if (gp->mode & ATTR_WDUMMY)
				continue;

			ptr += utf8encode(gp->u, ptr);
		}

		/*
		 * Copy and pasting of line endings is inconsistent
		 * in the inconsistent terminal and GUI world.
		 * The best solution seems like to produce '\n' when
		 * something is copied from st and convert '\n' to
		 * '\r', when something to be pasted is received by
		 * st.
		 * FIXME: Fix the computer world.
		 */
		if (
		#if VIM_BROWSE_PATCH
			(y < endy || lastx == term.col - 1)
		#else
			(y < sel.ne.y || lastx >= linelen)
		#endif //
		    && (!(last->mode & ATTR_WRAP) || sel.type == SEL_RECTANGULAR))
			*ptr++ = '\n';
	}
	*ptr = 0;
	return str;
}

void
selclear(void)
{
	if (sel.ob.x == -1)
		return;
	sel.mode = SEL_IDLE;
	sel.ob.x = -1;
	#if VIM_BROWSE_PATCH
	selnormalize();
	#else
	tsetdirt(sel.nb.y, sel.ne.y);
	#endif // VIM_BROWSE_PATCH
}

void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void
execsh(char *cmd, char **args)
{
	char *sh, *prog, *arg;
	const struct passwd *pw;

	errno = 0;
	if ((pw = getpwuid(getuid())) == NULL) {
		if (errno)
			die("getpwuid: %s\n", strerror(errno));
		else
			die("who are you?\n");
	}

	if ((sh = getenv("SHELL")) == NULL)
		sh = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

	if (args) {
		prog = args[0];
		arg = NULL;
	} else if (scroll) {
		prog = scroll;
		arg = utmp ? utmp : sh;
	} else if (utmp) {
		prog = utmp;
		arg = NULL;
	} else {
		prog = sh;
		arg = NULL;
	}
	DEFAULT(args, ((char *[]) {prog, arg, NULL}));

	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("SHELL", sh, 1);
	setenv("HOME", pw->pw_dir, 1);
	setenv("TERM", termname, 1);

	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGALRM, SIG_DFL);

	execvp(prog, args);
	_exit(1);
}

void
sigchld(int a)
{
	int stat;
	pid_t p;

	#if EXTERNALPIPEIN_PATCH && EXTERNALPIPE_PATCH
	if ((p = waitpid((extpipeactive ? -1 : pid), &stat, WNOHANG)) < 0)
	#else
	if ((p = waitpid(pid, &stat, WNOHANG)) < 0)
	#endif // EXTERNALPIPEIN_PATCH
		die("waiting for pid %hd failed: %s\n", pid, strerror(errno));

	#if EXTERNALPIPE_PATCH
	if (pid != p) {
		if (!extpipeactive)
			return;

		if (p == 0 && wait(&stat) < 0)
			die("wait: %s\n", strerror(errno));

		/* reinstall sigchld handler */
		signal(SIGCHLD, sigchld);
		extpipeactive = 0;
		return;
	}
	#else
	if (pid != p)
		return;
	#endif // EXTERNALPIPE_PATCH

	#if EXTERNALPIPEIN_PATCH && EXTERNALPIPE_PATCH
	close(csdfd);
	#endif // EXTERNALPIPEIN_PATCH

	if (WIFEXITED(stat) && WEXITSTATUS(stat))
		die("child exited with status %d\n", WEXITSTATUS(stat));
	else if (WIFSIGNALED(stat))
		die("child terminated due to signal %d\n", WTERMSIG(stat));
	_exit(0);
}

void
stty(char **args)
{
	char cmd[_POSIX_ARG_MAX], **p, *q, *s;
	size_t n, siz;

	if ((n = strlen(stty_args)) > sizeof(cmd)-1)
		die("incorrect stty parameters\n");
	memcpy(cmd, stty_args, n);
	q = cmd + n;
	siz = sizeof(cmd) - n;
	for (p = args; p && (s = *p); ++p) {
		if ((n = strlen(s)) > siz-1)
			die("stty parameter length too long\n");
		*q++ = ' ';
		memcpy(q, s, n);
		q += n;
		siz -= n + 1;
	}
	*q = '\0';
	if (system(cmd) != 0)
		perror("Couldn't call stty");
}

int
ttynew(const char *line, char *cmd, const char *out, char **args)
{
	int m, s;
	#if EXTERNALPIPEIN_PATCH && EXTERNALPIPE_PATCH
	struct sigaction sa;
	#endif // EXTERNALPIPEIN_PATCH

	if (out) {
		term.mode |= MODE_PRINT;
		iofd = (!strcmp(out, "-")) ?
			  1 : open(out, O_WRONLY | O_CREAT, 0666);
		if (iofd < 0) {
			fprintf(stderr, "Error opening %s:%s\n",
				out, strerror(errno));
		}
	}

	if (line) {
		if ((cmdfd = open(line, O_RDWR)) < 0)
			die("open line '%s' failed: %s\n",
			    line, strerror(errno));
		dup2(cmdfd, 0);
		stty(args);
		return cmdfd;
	}

	/* seems to work fine on linux, openbsd and freebsd */
	if (openpty(&m, &s, NULL, NULL, NULL) < 0)
		die("openpty failed: %s\n", strerror(errno));

	switch (pid = fork()) {
	case -1:
		die("fork failed: %s\n", strerror(errno));
		break;
	case 0:
		close(iofd);
		close(m);
		setsid(); /* create a new process group */
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		if (ioctl(s, TIOCSCTTY, NULL) < 0)
			die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
		close(s);
		close(m);
		if (s > 2)
				close(s);
#ifdef __OpenBSD__
		if (pledge("stdio getpw proc exec", NULL) == -1)
			die("pledge\n");
#endif
		execsh(cmd, args);
		break;
	default:
#ifdef __OpenBSD__
		#if RIGHTCLICKTOPLUMB_PATCH || OPENCOPIED_PATCH
		if (pledge("stdio rpath tty proc ps exec", NULL) == -1)
		#else
		if (pledge("stdio rpath tty proc", NULL) == -1)
		#endif // RIGHTCLICKTOPLUMB_PATCH
			die("pledge\n");
#endif
		close(s);
		cmdfd = m;
		#if EXTERNALPIPEIN_PATCH && EXTERNALPIPE_PATCH
		csdfd = s;
		memset(&sa, 0, sizeof(sa));
		sigemptyset(&sa.sa_mask);
		sa.sa_handler = sigchld;
		sigaction(SIGCHLD, &sa, NULL);
		#else
		signal(SIGCHLD, sigchld);
		#endif // EXTERNALPIPEIN_PATCH
		break;
	}
	return cmdfd;
}

size_t
ttyread(void)
{
	static char buf[BUFSIZ];
	static int buflen = 0;
	int ret, written;

	/* append read bytes to unprocessed bytes */
	#if SYNC_PATCH
	ret = twrite_aborted ? 1 : read(cmdfd, buf+buflen, LEN(buf)-buflen);
	#else
	ret = read(cmdfd, buf+buflen, LEN(buf)-buflen);
	#endif // SYNC_PATCH

	switch (ret) {
	case 0:
		exit(0);
	case -1:
		die("couldn't read from shell: %s\n", strerror(errno));
	default:
		#if SYNC_PATCH
		buflen += twrite_aborted ? 0 : ret;
		#else
		buflen += ret;
		#endif // SYNC_PATCH
		written = twrite(buf, buflen, 0);
		buflen -= written;
		/* keep any incomplete UTF-8 byte sequence for the next call */
		if (buflen > 0)
			memmove(buf, buf + written, buflen);
		return ret;
	}
}

void
ttywrite(const char *s, size_t n, int may_echo)
{
	const char *next;
	#if SCROLLBACK_PATCH
	Arg arg = (Arg) { .i = term.scr };

	kscrolldown(&arg);
	#endif // SCROLLBACK_PATCH

	if (may_echo && IS_SET(MODE_ECHO))
		twrite(s, n, 1);

	if (!IS_SET(MODE_CRLF)) {
		ttywriteraw(s, n);
		return;
	}

	/* This is similar to how the kernel handles ONLCR for ttys */
	while (n > 0) {
		if (*s == '\r') {
			next = s + 1;
			ttywriteraw("\r\n", 2);
		} else {
			next = memchr(s, '\r', n);
			DEFAULT(next, s + n);
			ttywriteraw(s, next - s);
		}
		n -= next - s;
		s = next;
	}
}

void
ttywriteraw(const char *s, size_t n)
{
	fd_set wfd, rfd;
	ssize_t r;
	size_t lim = 256;

	/*
	 * Remember that we are using a pty, which might be a modem line.
	 * Writing too much will clog the line. That's why we are doing this
	 * dance.
	 * FIXME: Migrate the world to Plan 9.
	 */
	while (n > 0) {
		FD_ZERO(&wfd);
		FD_ZERO(&rfd);
		FD_SET(cmdfd, &wfd);
		FD_SET(cmdfd, &rfd);

		/* Check if we can write. */
		if (pselect(cmdfd+1, &rfd, &wfd, NULL, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			die("select failed: %s\n", strerror(errno));
		}
		if (FD_ISSET(cmdfd, &wfd)) {
			/*
			 * Only write the bytes written by ttywrite() or the
			 * default of 256. This seems to be a reasonable value
			 * for a serial line. Bigger values might clog the I/O.
			 */
			if ((r = write(cmdfd, s, (n < lim)? n : lim)) < 0)
				goto write_error;
			if (r < n) {
				/*
				 * We weren't able to write out everything.
				 * This means the buffer is getting full
				 * again. Empty it.
				 */
				if (n < lim)
					lim = ttyread();
				n -= r;
				s += r;
			} else {
				/* All bytes have been written. */
				break;
			}
		}
		if (FD_ISSET(cmdfd, &rfd))
			lim = ttyread();
	}
	return;

write_error:
	die("write error on tty: %s\n", strerror(errno));
}

void
ttyresize(int tw, int th)
{
	struct winsize w;

	w.ws_row = term.row;
	w.ws_col = term.col;
	w.ws_xpixel = tw;
	w.ws_ypixel = th;
	if (ioctl(cmdfd, TIOCSWINSZ, &w) < 0)
		fprintf(stderr, "Couldn't set window size: %s\n", strerror(errno));
}

void
ttyhangup()
{
	/* Send SIGHUP to shell */
	kill(pid, SIGHUP);
}

int
tattrset(int attr)
{
	int i, j;

	for (i = 0; i < term.row-1; i++) {
		for (j = 0; j < term.col-1; j++) {
			if (term.line[i][j].mode & attr)
				return 1;
		}
	}

	return 0;
}

void
tsetdirt(int top, int bot)
{
	int i;

	LIMIT(top, 0, term.row-1);
	LIMIT(bot, 0, term.row-1);

	for (i = top; i <= bot; i++)
		term.dirty[i] = 1;
}

void
tsetdirtattr(int attr)
{
	int i, j;

	for (i = 0; i < term.row-1; i++) {
		for (j = 0; j < term.col-1; j++) {
			if (term.line[i][j].mode & attr) {
				tsetdirt(i, i);
				break;
			}
		}
	}
}

void
tfulldirt(void)
{
	#if SYNC_PATCH
	tsync_end();
	#endif // SYNC_PATCH
	tsetdirt(0, term.row-1);
}

void
tcursor(int mode)
{
	#if VIM_BROWSE_PATCH
	int alt = (histOp) ? 0 : (IS_SET(MODE_ALTSCREEN) + 1);
	#else
	static TCursor c[2];
	int alt = IS_SET(MODE_ALTSCREEN);
	#endif // VIM_BROWSE_PATCH

	if (mode == CURSOR_SAVE) {
		c[alt] = term.c;
	} else if (mode == CURSOR_LOAD) {
		term.c = c[alt];
		tmoveto(c[alt].x, c[alt].y);
	}
}

void
treset(void)
{
	uint i;
	#if SIXEL_PATCH
	ImageList *im;
	#endif // SIXEL_PATCH

	term.c = (TCursor){{
		.mode = ATTR_NULL,
		.fg = defaultfg,
		.bg = defaultbg
	}, .x = 0, .y = 0, .state = CURSOR_DEFAULT};

	memset(term.tabs, 0, term.col * sizeof(*term.tabs));
	for (i = tabspaces; i < term.col; i += tabspaces)
		term.tabs[i] = 1;
	term.top = 0;
	term.bot = term.row - 1;
	term.mode = MODE_WRAP|MODE_UTF8;
	memset(term.trantbl, CS_USA, sizeof(term.trantbl));
	term.charset = 0;

	for (i = 0; i < 2; i++) {
		tmoveto(0, 0);
		tcursor(CURSOR_SAVE);
		tclearregion(0, 0, term.col-1, term.row-1);
		tswapscreen();
	}
	#if SIXEL_PATCH
	for (im = term.images; im; im = im->next)
		im->should_delete = 1;
	#endif // SIXEL_PATCH
}

void
tnew(int col, int row)
{
	term = (Term){ .c = { .attr = { .fg = defaultfg, .bg = defaultbg } } };
	tresize(col, row);
	treset();
}

void
tswapscreen(void)
{
	Line *tmp = term.line;
	#if SIXEL_PATCH
	ImageList *im = term.images;
	#endif // SIXEL_PATCH

	term.line = term.alt;
	term.alt = tmp;
	#if SIXEL_PATCH
	term.images = term.images_alt;
	term.images_alt = im;
	#endif // SIXEL_PATCH
	term.mode ^= MODE_ALTSCREEN;
	tfulldirt();
}

void
#if SCROLLBACK_PATCH
tscrolldown(int orig, int n, int copyhist)
#else
tscrolldown(int orig, int n)
#endif // SCROLLBACK_PATCH
{
	#if VIM_BROWSE_PATCH
	if (!orig && historyBufferScroll(-n))
		return;
	#endif // VIM_BROWSE_PATCH
	int i;
	Line temp;

	LIMIT(n, 0, term.bot-orig+1);

	#if SCROLLBACK_PATCH
	if (copyhist) {
		term.histi = (term.histi - 1 + HISTSIZE) % HISTSIZE;
		temp = term.hist[term.histi];
		term.hist[term.histi] = term.line[term.bot];
		term.line[term.bot] = temp;
	}
	#endif // SCROLLBACK_PATCH

	tsetdirt(orig, term.bot-n);
	tclearregion(0, term.bot-n+1, term.col-1, term.bot);

	for (i = term.bot; i >= orig+n; i--) {
		temp = term.line[i];
		term.line[i] = term.line[i-n];
		term.line[i-n] = temp;
	}

	#if SIXEL_PATCH
	scroll_images(n);
	#endif // SIXEL_PATCH

	#if SCROLLBACK_PATCH
	if (term.scr == 0)
		selscroll(orig, n);
	#else
	selscroll(orig, n);
	#endif // SCROLLBACK_PATCH
}

void
#if SCROLLBACK_PATCH
tscrollup(int orig, int n, int copyhist)
#else
tscrollup(int orig, int n)
#endif // SCROLLBACK_PATCH
{
	#if VIM_BROWSE_PATCH
	if (!orig && historyBufferScroll(n))
		return;
	#endif // VIM_BROWSE_PATCH
	int i;
	Line temp;

	LIMIT(n, 0, term.bot-orig+1);

	#if SCROLLBACK_PATCH
	if (copyhist) {
		term.histi = (term.histi + 1) % HISTSIZE;
		temp = term.hist[term.histi];
		term.hist[term.histi] = term.line[orig];
		term.line[orig] = temp;
	}

	if (term.scr > 0 && term.scr < HISTSIZE)
		term.scr = MIN(term.scr + n, HISTSIZE-1);
	#endif // SCROLLBACK_PATCH

	tclearregion(0, orig, term.col-1, orig+n-1);
	tsetdirt(orig+n, term.bot);

	for (i = orig; i <= term.bot-n; i++) {
		temp = term.line[i];
		term.line[i] = term.line[i+n];
		term.line[i+n] = temp;
	}

	#if SIXEL_PATCH
	scroll_images(-1 * n);
	#endif // SIXEL_PATCH

	#if SCROLLBACK_PATCH
	if (term.scr == 0)
		selscroll(orig, -n);
	#else
	selscroll(orig, -n);
	#endif // SCROLLBACK_PATCH
}

void
selscroll(int orig, int n)
{
	if (sel.ob.x == -1)
		return;

	if (BETWEEN(sel.nb.y, orig, term.bot) != BETWEEN(sel.ne.y, orig, term.bot)) {
		selclear();
	} else if (BETWEEN(sel.nb.y, orig, term.bot)) {
		sel.ob.y += n;
		sel.oe.y += n;
		if (sel.ob.y < term.top || sel.ob.y > term.bot ||
		    sel.oe.y < term.top || sel.oe.y > term.bot) {
			selclear();
		} else {
			selnormalize();
		}
	}
}

void
tnewline(int first_col)
{
	int y = term.c.y;

	if (y == term.bot) {
		#if SCROLLBACK_PATCH
		tscrollup(term.top, 1, 1);
		#else
		tscrollup(term.top, 1);
		#endif // SCROLLBACK_PATCH
	} else {
		y++;
	}
	tmoveto(first_col ? 0 : term.c.x, y);
}

#if UNDERCURL_PATCH
void
readcolonargs(char **p, int cursor, int params[][CAR_PER_ARG])
{
	int i = 0;
	for (; i < CAR_PER_ARG; i++)
		params[cursor][i] = -1;

	if (**p != ':')
		return;

	char *np = NULL;
	i = 0;

	while (**p == ':' && i < CAR_PER_ARG) {
		while (**p == ':')
			(*p)++;
		params[cursor][i] = strtol(*p, &np, 10);
		*p = np;
		i++;
	}
}
#endif // UNDERCURL_PATCH

void
csiparse(void)
{
	char *p = csiescseq.buf, *np;
	long int v;

	csiescseq.narg = 0;
	if (*p == '?') {
		csiescseq.priv = 1;
		p++;
	}

	csiescseq.buf[csiescseq.len] = '\0';
	while (p < csiescseq.buf+csiescseq.len) {
		np = NULL;
		v = strtol(p, &np, 10);
		if (np == p)
			v = 0;
		if (v == LONG_MAX || v == LONG_MIN)
			v = -1;
		csiescseq.arg[csiescseq.narg++] = v;
		p = np;
		#if UNDERCURL_PATCH
		readcolonargs(&p, csiescseq.narg-1, csiescseq.carg);
		#endif // UNDERCURL_PATCH
		if (*p != ';' || csiescseq.narg == ESC_ARG_SIZ)
			break;
		p++;
	}
	csiescseq.mode[0] = *p++;
	csiescseq.mode[1] = (p < csiescseq.buf+csiescseq.len) ? *p : '\0';
}

/* for absolute user moves, when decom is set */
void
tmoveato(int x, int y)
{
	tmoveto(x, y + ((term.c.state & CURSOR_ORIGIN) ? term.top: 0));
}

void
tmoveto(int x, int y)
{
	int miny, maxy;

	if (term.c.state & CURSOR_ORIGIN) {
		miny = term.top;
		maxy = term.bot;
	} else {
		miny = 0;
		maxy = term.row - 1;
	}
	term.c.state &= ~CURSOR_WRAPNEXT;
	term.c.x = LIMIT(x, 0, term.col-1);
	term.c.y = LIMIT(y, miny, maxy);
}

void
tsetchar(Rune u, const Glyph *attr, int x, int y)
{
	static const char *vt100_0[62] = { /* 0x41 - 0x7e */
		"↑", "↓", "→", "←", "█", "▚", "☃", /* A - G */
		0, 0, 0, 0, 0, 0, 0, 0, /* H - O */
		0, 0, 0, 0, 0, 0, 0, 0, /* P - W */
		0, 0, 0, 0, 0, 0, 0, " ", /* X - _ */
		"◆", "▒", "␉", "␌", "␍", "␊", "°", "±", /* ` - g */
		"␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺", /* h - o */
		"⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬", /* p - w */
		"│", "≤", "≥", "π", "≠", "£", "·", /* x - ~ */
	};

	/*
	 * The table is proudly stolen from rxvt.
	 */
	if (term.trantbl[term.charset] == CS_GRAPHIC0 &&
	   BETWEEN(u, 0x41, 0x7e) && vt100_0[u - 0x41])
		utf8decode(vt100_0[u - 0x41], &u, UTF_SIZ);

	if (term.line[y][x].mode & ATTR_WIDE) {
		if (x+1 < term.col) {
			term.line[y][x+1].u = ' ';
			term.line[y][x+1].mode &= ~ATTR_WDUMMY;
		}
	} else if (term.line[y][x].mode & ATTR_WDUMMY) {
		term.line[y][x-1].u = ' ';
		term.line[y][x-1].mode &= ~ATTR_WIDE;
	}

	term.dirty[y] = 1;
	term.line[y][x] = *attr;
	term.line[y][x].u = u;

	#if BOXDRAW_PATCH
	if (isboxdraw(u))
		term.line[y][x].mode |= ATTR_BOXDRAW;
	#endif // BOXDRAW_PATCH
}

void
tclearregion(int x1, int y1, int x2, int y2)
{
	int x, y, temp;
	Glyph *gp;

	if (x1 > x2)
		temp = x1, x1 = x2, x2 = temp;
	if (y1 > y2)
		temp = y1, y1 = y2, y2 = temp;

	#if VIM_BROWSE_PATCH
	LIMIT(x1, 0, buffCols-1);
	LIMIT(x2, 0, buffCols-1);
	#elif COLUMNS_PATCH
	LIMIT(x1, 0, term.maxcol-1);
	LIMIT(x2, 0, term.maxcol-1);
	#else
	LIMIT(x1, 0, term.col-1);
	LIMIT(x2, 0, term.col-1);
	#endif // VIM_BROWSE_PATCH
	LIMIT(y1, 0, term.row-1);
	LIMIT(y2, 0, term.row-1);

	for (y = y1; y <= y2; y++) {
		term.dirty[y] = 1;
		for (x = x1; x <= x2; x++) {
			gp = &term.line[y][x];
			if (selected(x, y))
				selclear();
			gp->fg = term.c.attr.fg;
			gp->bg = term.c.attr.bg;
			gp->mode = 0;
			gp->u = ' ';
		}
	}
}

void
tdeletechar(int n)
{
	int dst, src, size;
	Glyph *line;

	LIMIT(n, 0, term.col - term.c.x);

	dst = term.c.x;
	src = term.c.x + n;
	size = term.col - src;
	line = term.line[term.c.y];

	memmove(&line[dst], &line[src], size * sizeof(Glyph));
	tclearregion(term.col-n, term.c.y, term.col-1, term.c.y);
}

void
tinsertblank(int n)
{
	int dst, src, size;
	Glyph *line;

	LIMIT(n, 0, term.col - term.c.x);

	dst = term.c.x + n;
	src = term.c.x;
	size = term.col - dst;
	line = term.line[term.c.y];

	memmove(&line[dst], &line[src], size * sizeof(Glyph));
	tclearregion(src, term.c.y, dst - 1, term.c.y);
}

void
tinsertblankline(int n)
{
	if (BETWEEN(term.c.y, term.top, term.bot))
		#if SCROLLBACK_PATCH
		tscrolldown(term.c.y, n, 0);
		#else
		tscrolldown(term.c.y, n);
		#endif // SCROLLBACK_PATCH
}

void
tdeleteline(int n)
{
	if (BETWEEN(term.c.y, term.top, term.bot))
		#if SCROLLBACK_PATCH
		tscrollup(term.c.y, n, 0);
		#else
		tscrollup(term.c.y, n);
		#endif // SCROLLBACK_PATCH
}

int32_t
tdefcolor(const int *attr, int *npar, int l)
{
	int32_t idx = -1;
	uint r, g, b;

	switch (attr[*npar + 1]) {
	case 2: /* direct color in RGB space */
		if (*npar + 4 >= l) {
			fprintf(stderr,
				"erresc(38): Incorrect number of parameters (%d)\n",
				*npar);
			break;
		}
		r = attr[*npar + 2];
		g = attr[*npar + 3];
		b = attr[*npar + 4];
		*npar += 4;
		if (!BETWEEN(r, 0, 255) || !BETWEEN(g, 0, 255) || !BETWEEN(b, 0, 255))
			fprintf(stderr, "erresc: bad rgb color (%u,%u,%u)\n",
				r, g, b);
		else
			idx = TRUECOLOR(r, g, b);
		break;
	case 5: /* indexed color */
		if (*npar + 2 >= l) {
			fprintf(stderr,
				"erresc(38): Incorrect number of parameters (%d)\n",
				*npar);
			break;
		}
		*npar += 2;
		if (!BETWEEN(attr[*npar], 0, 255))
			fprintf(stderr, "erresc: bad fgcolor %d\n", attr[*npar]);
		else
			idx = attr[*npar];
		break;
	case 0: /* implemented defined (only foreground) */
	case 1: /* transparent */
	case 3: /* direct color in CMY space */
	case 4: /* direct color in CMYK space */
	default:
		fprintf(stderr,
		        "erresc(38): gfx attr %d unknown\n", attr[*npar]);
		break;
	}

	return idx;
}

void
tsetattr(const int *attr, int l)
{
	int i;
	int32_t idx;

	for (i = 0; i < l; i++) {
		switch (attr[i]) {
		case 0:
			term.c.attr.mode &= ~(
				ATTR_BOLD       |
				ATTR_FAINT      |
				ATTR_ITALIC     |
				ATTR_UNDERLINE  |
				ATTR_BLINK      |
				ATTR_REVERSE    |
				ATTR_INVISIBLE  |
				ATTR_STRUCK     );
			term.c.attr.fg = defaultfg;
			term.c.attr.bg = defaultbg;
			#if UNDERCURL_PATCH
			term.c.attr.ustyle = -1;
			term.c.attr.ucolor[0] = -1;
			term.c.attr.ucolor[1] = -1;
			term.c.attr.ucolor[2] = -1;
			#endif // UNDERCURL_PATCH
			break;
		case 1:
			term.c.attr.mode |= ATTR_BOLD;
			break;
		case 2:
			term.c.attr.mode |= ATTR_FAINT;
			break;
		case 3:
			term.c.attr.mode |= ATTR_ITALIC;
			break;
		case 4:
			#if UNDERCURL_PATCH
			term.c.attr.ustyle = csiescseq.carg[i][0];

			if (term.c.attr.ustyle != 0)
				term.c.attr.mode |= ATTR_UNDERLINE;
			else
				term.c.attr.mode &= ~ATTR_UNDERLINE;

			term.c.attr.mode ^= ATTR_DIRTYUNDERLINE;
			#else
			term.c.attr.mode |= ATTR_UNDERLINE;
			#endif // UNDERCURL_PATCH
			break;
		case 5: /* slow blink */
			/* FALLTHROUGH */
		case 6: /* rapid blink */
			term.c.attr.mode |= ATTR_BLINK;
			break;
		case 7:
			term.c.attr.mode |= ATTR_REVERSE;
			break;
		case 8:
			term.c.attr.mode |= ATTR_INVISIBLE;
			break;
		case 9:
			term.c.attr.mode |= ATTR_STRUCK;
			break;
		case 22:
			term.c.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT);
			break;
		case 23:
			term.c.attr.mode &= ~ATTR_ITALIC;
			break;
		case 24:
			term.c.attr.mode &= ~ATTR_UNDERLINE;
			break;
		case 25:
			term.c.attr.mode &= ~ATTR_BLINK;
			break;
		case 27:
			term.c.attr.mode &= ~ATTR_REVERSE;
			break;
		case 28:
			term.c.attr.mode &= ~ATTR_INVISIBLE;
			break;
		case 29:
			term.c.attr.mode &= ~ATTR_STRUCK;
			break;
		case 38:
			if ((idx = tdefcolor(attr, &i, l)) >= 0)
				#if MONOCHROME_PATCH
				term.c.attr.fg = defaultfg;
				#else
				term.c.attr.fg = idx;
				#endif // MONOCHROME_PATCH
			break;
		case 39:
			term.c.attr.fg = defaultfg;
			break;
		case 48:
			if ((idx = tdefcolor(attr, &i, l)) >= 0)
				#if MONOCHROME_PATCH
				term.c.attr.bg = 0;
				#else
				term.c.attr.bg = idx;
				#endif // MONOCHROME_PATCH
			break;
		case 49:
			term.c.attr.bg = defaultbg;
			break;
		#if UNDERCURL_PATCH
		case 58:
			term.c.attr.ucolor[0] = csiescseq.carg[i][1];
			term.c.attr.ucolor[1] = csiescseq.carg[i][2];
			term.c.attr.ucolor[2] = csiescseq.carg[i][3];
			term.c.attr.mode ^= ATTR_DIRTYUNDERLINE;
			break;
		case 59:
			term.c.attr.ucolor[0] = -1;
			term.c.attr.ucolor[1] = -1;
			term.c.attr.ucolor[2] = -1;
			term.c.attr.mode ^= ATTR_DIRTYUNDERLINE;
			break;
		#endif // UNDERCURL_PATCH
		default:
			if (BETWEEN(attr[i], 30, 37)) {
				#if MONOCHROME_PATCH
				term.c.attr.fg = defaultfg;
				#else
				term.c.attr.fg = attr[i] - 30;
				#endif // MONOCHROME_PATCH
			} else if (BETWEEN(attr[i], 40, 47)) {
				#if MONOCHROME_PATCH
				term.c.attr.bg = 0;
				#else
				term.c.attr.bg = attr[i] - 40;
				#endif // MONOCHROME_PATCH
			} else if (BETWEEN(attr[i], 90, 97)) {
				#if MONOCHROME_PATCH
				term.c.attr.fg = defaultfg;
				#else
				term.c.attr.fg = attr[i] - 90 + 8;
				#endif // MONOCHROME_PATCH
			} else if (BETWEEN(attr[i], 100, 107)) {
				#if MONOCHROME_PATCH
				term.c.attr.bg = 0;
				#else
				term.c.attr.bg = attr[i] - 100 + 8;
				#endif // MONOCHROME_PATCH
			} else {
				fprintf(stderr,
					"erresc(default): gfx attr %d unknown\n",
					attr[i]);
				csidump();
			}
			break;
		}
	}
}

void
tsetscroll(int t, int b)
{
	int temp;

	LIMIT(t, 0, term.row-1);
	LIMIT(b, 0, term.row-1);
	if (t > b) {
		temp = t;
		t = b;
		b = temp;
	}
	term.top = t;
	term.bot = b;
}

void
tsetmode(int priv, int set, const int *args, int narg)
{
	int alt;
	const int *lim;

	for (lim = args + narg; args < lim; ++args) {
		if (priv) {
			switch (*args) {
			case 1: /* DECCKM -- Cursor key */
				xsetmode(set, MODE_APPCURSOR);
				break;
			case 5: /* DECSCNM -- Reverse video */
				xsetmode(set, MODE_REVERSE);
				break;
			case 6: /* DECOM -- Origin */
				MODBIT(term.c.state, set, CURSOR_ORIGIN);
				tmoveato(0, 0);
				break;
			case 7: /* DECAWM -- Auto wrap */
				MODBIT(term.mode, set, MODE_WRAP);
				break;
			case 0:  /* Error (IGNORED) */
			case 2:  /* DECANM -- ANSI/VT52 (IGNORED) */
			case 3:  /* DECCOLM -- Column  (IGNORED) */
			case 4:  /* DECSCLM -- Scroll (IGNORED) */
			case 8:  /* DECARM -- Auto repeat (IGNORED) */
			case 18: /* DECPFF -- Printer feed (IGNORED) */
			case 19: /* DECPEX -- Printer extent (IGNORED) */
			case 42: /* DECNRCM -- National characters (IGNORED) */
			case 12: /* att610 -- Start blinking cursor (IGNORED) */
				break;
			case 25: /* DECTCEM -- Text Cursor Enable Mode */
				xsetmode(!set, MODE_HIDE);
				break;
			case 9:    /* X10 mouse compatibility mode */
				xsetpointermotion(0);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEX10);
				break;
			case 1000: /* 1000: report button press */
				xsetpointermotion(0);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEBTN);
				break;
			case 1002: /* 1002: report motion on button press */
				xsetpointermotion(0);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEMOTION);
				break;
			case 1003: /* 1003: enable all mouse motions */
				xsetpointermotion(set);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEMANY);
				break;
			case 1004: /* 1004: send focus events to tty */
				xsetmode(set, MODE_FOCUS);
				break;
			case 1006: /* 1006: extended reporting mode */
				xsetmode(set, MODE_MOUSESGR);
				break;
			case 1034:
				xsetmode(set, MODE_8BIT);
				break;
			case 1049: /* swap screen & set/restore cursor as xterm */
				if (!allowaltscreen)
					break;
				tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
				/* FALLTHROUGH */
			case 47: /* swap screen */
			case 1047:
				if (!allowaltscreen)
					break;
				alt = IS_SET(MODE_ALTSCREEN);
				if (alt) {
					tclearregion(0, 0, term.col-1,
							term.row-1);
				}
				if (set ^ alt) /* set is always 1 or 0 */
					tswapscreen();
				if (*args != 1049)
					break;
				/* FALLTHROUGH */
			case 1048:
				tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
				break;
			case 2004: /* 2004: bracketed paste mode */
				xsetmode(set, MODE_BRCKTPASTE);
				break;
			/* Not implemented mouse modes. See comments there. */
			case 1001: /* mouse highlight mode; can hang the
				      terminal by design when implemented. */
			case 1005: /* UTF-8 mouse mode; will confuse
				      applications not supporting UTF-8
				      and luit. */
			case 1015: /* urxvt mangled mouse mode; incompatible
				      and can be mistaken for other control
				      codes. */
				break;
			default:
				fprintf(stderr,
					"erresc: unknown private set/reset mode %d\n",
					*args);
				break;
			}
		} else {
			switch (*args) {
			case 0:  /* Error (IGNORED) */
				break;
			case 2:
				xsetmode(set, MODE_KBDLOCK);
				break;
			case 4:  /* IRM -- Insertion-replacement */
				MODBIT(term.mode, set, MODE_INSERT);
				break;
			case 12: /* SRM -- Send/Receive */
				MODBIT(term.mode, !set, MODE_ECHO);
				break;
			case 20: /* LNM -- Linefeed/new line */
				MODBIT(term.mode, set, MODE_CRLF);
				break;
			default:
				fprintf(stderr,
					"erresc: unknown set/reset mode %d\n",
					*args);
				break;
			}
		}
	}
}

void
csihandle(void)
{
	char buf[40];
	int len;
	#if SIXEL_PATCH
	ImageList *im;
	#endif // SIXEL_PATCH

	switch (csiescseq.mode[0]) {
	default:
	unknown:
		fprintf(stderr, "erresc: unknown csi ");
		csidump();
		/* die(""); */
		break;
	case '@': /* ICH -- Insert <n> blank char */
		DEFAULT(csiescseq.arg[0], 1);
		tinsertblank(csiescseq.arg[0]);
		break;
	case 'A': /* CUU -- Cursor <n> Up */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x, term.c.y-csiescseq.arg[0]);
		break;
	case 'B': /* CUD -- Cursor <n> Down */
	case 'e': /* VPR --Cursor <n> Down */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x, term.c.y+csiescseq.arg[0]);
		break;
	case 'i': /* MC -- Media Copy */
		switch (csiescseq.arg[0]) {
		case 0:
			tdump();
			break;
		case 1:
			tdumpline(term.c.y);
			break;
		case 2:
			tdumpsel();
			break;
		case 4:
			term.mode &= ~MODE_PRINT;
			break;
		case 5:
			term.mode |= MODE_PRINT;
			break;
		}
		break;
	case 'c': /* DA -- Device Attributes */
		if (csiescseq.arg[0] == 0)
			ttywrite(vtiden, strlen(vtiden), 0);
		break;
	case 'b': /* REP -- if last char is printable print it <n> more times */
		DEFAULT(csiescseq.arg[0], 1);
		if (term.lastc)
			while (csiescseq.arg[0]-- > 0)
				tputc(term.lastc);
		break;
	case 'C': /* CUF -- Cursor <n> Forward */
	case 'a': /* HPR -- Cursor <n> Forward */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x+csiescseq.arg[0], term.c.y);
		break;
	case 'D': /* CUB -- Cursor <n> Backward */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x-csiescseq.arg[0], term.c.y);
		break;
	case 'E': /* CNL -- Cursor <n> Down and first col */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(0, term.c.y+csiescseq.arg[0]);
		break;
	case 'F': /* CPL -- Cursor <n> Up and first col */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(0, term.c.y-csiescseq.arg[0]);
		break;
	case 'g': /* TBC -- Tabulation clear */
		switch (csiescseq.arg[0]) {
		case 0: /* clear current tab stop */
			term.tabs[term.c.x] = 0;
			break;
		case 3: /* clear all the tabs */
			memset(term.tabs, 0, term.col * sizeof(*term.tabs));
			break;
		default:
			goto unknown;
		}
		break;
	case 'G': /* CHA -- Move to <col> */
	case '`': /* HPA */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(csiescseq.arg[0]-1, term.c.y);
		break;
	case 'H': /* CUP -- Move to <row> <col> */
	case 'f': /* HVP */
		DEFAULT(csiescseq.arg[0], 1);
		DEFAULT(csiescseq.arg[1], 1);
		tmoveato(csiescseq.arg[1]-1, csiescseq.arg[0]-1);
		break;
	case 'I': /* CHT -- Cursor Forward Tabulation <n> tab stops */
		DEFAULT(csiescseq.arg[0], 1);
		tputtab(csiescseq.arg[0]);
		break;
	case 'J': /* ED -- Clear screen */
		switch (csiescseq.arg[0]) {
		case 0: /* below */
			tclearregion(term.c.x, term.c.y, term.col-1, term.c.y);
			if (term.c.y < term.row-1) {
				tclearregion(0, term.c.y+1, term.col-1,
						term.row-1);
			}
			break;
		case 1: /* above */
			if (term.c.y > 1)
				tclearregion(0, 0, term.col-1, term.c.y-1);
			tclearregion(0, term.c.y, term.c.x, term.c.y);
			break;
		case 2: /* screen */
			tclearregion(0, 0, term.col-1, term.row-1);
			break;
		case 3: /* all including scrollback */
			tclearregion(0, 0, term.col-1, term.row-1);

			#if SCROLLBACK_PATCH
			term.scr = 0;
			term.histi = 0;
			for (int i = 0; i < HISTSIZE; i++)
				term.hist[i][0].u = '\0';
			#endif // SCROLLBACK_PATCH

			#if SIXEL_PATCH
			for (im = term.images; im; im = im->next)
				im->should_delete = 1;
			#endif // SIXEL_PATCH
			break;
		default:
			goto unknown;
		}
		break;
	case 'K': /* EL -- Clear line */
		switch (csiescseq.arg[0]) {
		case 0: /* right */
			tclearregion(term.c.x, term.c.y, term.col-1,
					term.c.y);
			break;
		case 1: /* left */
			tclearregion(0, term.c.y, term.c.x, term.c.y);
			break;
		case 2: /* all */
			tclearregion(0, term.c.y, term.col-1, term.c.y);
			break;
		}
		break;
	case 'S': /* SU -- Scroll <n> line up */
		DEFAULT(csiescseq.arg[0], 1);
		#if SIXEL_PATCH && SCROLLBACK_PATCH
		tscrollup(term.top, csiescseq.arg[0], 1);
		#elif SCROLLBACK_PATCH
		tscrollup(term.top, csiescseq.arg[0], 0);
		#else
		tscrollup(term.top, csiescseq.arg[0]);
		#endif // SCROLLBACK_PATCH
		break;
	case 'T': /* SD -- Scroll <n> line down */
		DEFAULT(csiescseq.arg[0], 1);
		#if SIXEL_PATCH && SCROLLBACK_PATCH
		tscrolldown(term.top, csiescseq.arg[0], 1);
		#elif SCROLLBACK_PATCH
		tscrolldown(term.top, csiescseq.arg[0], 0);
		#else
		tscrolldown(term.top, csiescseq.arg[0]);
		#endif // SCROLLBACK_PATCH
		break;
	case 'L': /* IL -- Insert <n> blank lines */
		DEFAULT(csiescseq.arg[0], 1);
		tinsertblankline(csiescseq.arg[0]);
		break;
	case 'l': /* RM -- Reset Mode */
		tsetmode(csiescseq.priv, 0, csiescseq.arg, csiescseq.narg);
		break;
	case 'M': /* DL -- Delete <n> lines */
		DEFAULT(csiescseq.arg[0], 1);
		tdeleteline(csiescseq.arg[0]);
		break;
	case 'X': /* ECH -- Erase <n> char */
		DEFAULT(csiescseq.arg[0], 1);
		tclearregion(term.c.x, term.c.y,
				term.c.x + csiescseq.arg[0] - 1, term.c.y);
		break;
	case 'P': /* DCH -- Delete <n> char */
		DEFAULT(csiescseq.arg[0], 1);
		tdeletechar(csiescseq.arg[0]);
		break;
	case 'Z': /* CBT -- Cursor Backward Tabulation <n> tab stops */
		DEFAULT(csiescseq.arg[0], 1);
		tputtab(-csiescseq.arg[0]);
		break;
	case 'd': /* VPA -- Move to <row> */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveato(term.c.x, csiescseq.arg[0]-1);
		break;
	case 'h': /* SM -- Set terminal mode */
		tsetmode(csiescseq.priv, 1, csiescseq.arg, csiescseq.narg);
		break;
	case 'm': /* SGR -- Terminal attribute (color) */
		tsetattr(csiescseq.arg, csiescseq.narg);
		break;
	case 'n': /* DSR – Device Status Report (cursor position) */
		if (csiescseq.arg[0] == 6) {
			len = snprintf(buf, sizeof(buf), "\033[%i;%iR",
					term.c.y+1, term.c.x+1);
			ttywrite(buf, len, 0);
		}
		break;
	case 'r': /* DECSTBM -- Set Scrolling Region */
		if (csiescseq.priv) {
			goto unknown;
		} else {
			DEFAULT(csiescseq.arg[0], 1);
			DEFAULT(csiescseq.arg[1], term.row);
			tsetscroll(csiescseq.arg[0]-1, csiescseq.arg[1]-1);
			tmoveato(0, 0);
		}
		break;
	case 's': /* DECSC -- Save cursor position (ANSI.SYS) */
		tcursor(CURSOR_SAVE);
		break;
	#if CSI_22_23_PATCH
	case 't': /* title stack operations */
		switch (csiescseq.arg[0]) {
		case 22: /* pust current title on stack */
			switch (csiescseq.arg[1]) {
			case 0:
			case 1:
			case 2:
				xpushtitle();
				break;
			default:
				goto unknown;
			}
			break;
		case 23: /* pop last title from stack */
			switch (csiescseq.arg[1]) {
			case 0:
			case 1:
			case 2:
				xsettitle(NULL, 1);
				break;
			default:
				goto unknown;
			}
			break;
		default:
			goto unknown;
		}
		break;
	#endif // CSI_22_23_PATCH
	case 'u': /* DECRC -- Restore cursor position (ANSI.SYS) */
		tcursor(CURSOR_LOAD);
		break;
	case ' ':
		switch (csiescseq.mode[1]) {
		case 'q': /* DECSCUSR -- Set Cursor Style */
			if (xsetcursor(csiescseq.arg[0]))
				goto unknown;
			break;
		default:
			goto unknown;
		}
		break;
	}
}

void
csidump(void)
{
	size_t i;
	uint c;

	fprintf(stderr, "ESC[");
	for (i = 0; i < csiescseq.len; i++) {
		c = csiescseq.buf[i] & 0xff;
		if (isprint(c)) {
			putc(c, stderr);
		} else if (c == '\n') {
			fprintf(stderr, "(\\n)");
		} else if (c == '\r') {
			fprintf(stderr, "(\\r)");
		} else if (c == 0x1b) {
			fprintf(stderr, "(\\e)");
		} else {
			fprintf(stderr, "(%02x)", c);
		}
	}
	putc('\n', stderr);
}

void
csireset(void)
{
	memset(&csiescseq, 0, sizeof(csiescseq));
}

void
strhandle(void)
{
	char *p = NULL, *dec;
	int j, narg, par;
	#if SIXEL_PATCH
	ImageList *new_image;
	int i;
	#endif // SIXEL_PATCH

	term.esc &= ~(ESC_STR_END|ESC_STR);
	strparse();
	par = (narg = strescseq.narg) ? atoi(strescseq.args[0]) : 0;

	switch (strescseq.type) {
	case ']': /* OSC -- Operating System Command */
		switch (par) {
		case 0:
			if (narg > 1) {
				#if CSI_22_23_PATCH
				xsettitle(strescseq.args[1], 0);
				#else
				xsettitle(strescseq.args[1]);
				#endif // CSI_22_23_PATCH
				xseticontitle(strescseq.args[1]);
			}
			return;
		case 1:
			if (narg > 1)
				xseticontitle(strescseq.args[1]);
			return;
		case 2:
			if (narg > 1)
				#if CSI_22_23_PATCH
				xsettitle(strescseq.args[1], 0);
				#else
				xsettitle(strescseq.args[1]);
				#endif // CSI_22_23_PATCH
			return;
		case 52:
			if (narg > 2 && allowwindowops) {
				dec = base64dec(strescseq.args[2]);
				if (dec) {
					xsetsel(dec);
					xclipcopy();
				} else {
					fprintf(stderr, "erresc: invalid base64\n");
				}
			}
			return;
		case 10: /* foreground set */
			#if OSC_10_11_12_2_PATCH
			if (narg < 2)
				break;

			p = strescseq.args[1];
			if (xsetcolorname(defaultfg, p))
				fprintf(stderr, "erresc: invalid foreground color %d\n", p);
			else
				redraw();
			break;
			return;
			#endif // OSC_10_11_12_2_PATCH
		case 11: /* background set */
			#if OSC_10_11_12_2_PATCH
			if (narg < 2)
				break;

			p = strescseq.args[1];
			if (xsetcolorname(defaultbg, p))
				fprintf(stderr, "erresc: invalid background color %d\n", p);
			else
				redraw();
			break;
			return;
			#endif // OSC_10_11_12_2_PATCH
		case 12: /* cursor color */
			#if OSC_10_11_12_2_PATCH
			if (narg < 2)
				break;

			p = strescseq.args[1];
			if (xsetcolorname(defaultcs, p))
				fprintf(stderr, "erresc: invalid cursor color %d\n", p);
			else
				redraw();
			break;
			#endif // OSC_10_11_12_2_PATCH
		case 4: /* color set */
			if ((par == 4 && narg < 3) || narg < 2)
				break;
			p = strescseq.args[((par == 4) ? 2 : 1)];
			/* FALLTHROUGH */
		case 104: /* color reset, here p = NULL */
			if (par == 10)
				j = defaultfg;
			else if (par == 11)
				j = defaultbg;
			else if (par == 12)
				j = defaultcs;
			else
				j = (narg > 1) ? atoi(strescseq.args[1]) : -1;

			if (xsetcolorname(j, p)) {
				if (par == 104 && narg <= 1)
					return; /* color reset without parameter */
				fprintf(stderr, "erresc: invalid color j=%d, p=%s\n",
				        j, p ? p : "(null)");
			} else {
				if (j == defaultbg)
					xclearwin();
				redraw();
			}
			return;
		}
		break;
	case 'k': /* old title set compatibility */
		#if CSI_22_23_PATCH
		xsettitle(strescseq.args[0], 0);
		#else
		xsettitle(strescseq.args[0]);
		#endif // CSI_22_23_PATCH
		return;
	case 'P': /* DCS -- Device Control String */
		#if SIXEL_PATCH
		if (IS_SET(MODE_SIXEL)) {
			term.mode &= ~MODE_SIXEL;
			new_image = malloc(sizeof(ImageList));
			memset(new_image, 0, sizeof(ImageList));
			new_image->x = term.c.x;
			new_image->y = term.c.y;
			new_image->width = sixel_st.image.width;
			new_image->height = sixel_st.image.height;
			new_image->pixels = malloc(new_image->width * new_image->height * 4);
			if (sixel_parser_finalize(&sixel_st, new_image->pixels) != 0) {
				perror("sixel_parser_finalize() failed");
				sixel_parser_deinit(&sixel_st);
				return;
			}
			sixel_parser_deinit(&sixel_st);
			if (term.images) {
				ImageList *im;
				for (im = term.images; im->next;)
					im = im->next;
				im->next = new_image;
				new_image->prev = im;
			} else {
				term.images = new_image;
			}
			for (i = 0; i < (sixel_st.image.height + win.ch-1)/win.ch; ++i) {
				int x;
				tclearregion(term.c.x, term.c.y, term.c.x+(sixel_st.image.width+win.cw-1)/win.cw, term.c.y);
				for (x = term.c.x; x < MIN(term.col, term.c.x+(sixel_st.image.width+win.cw-1)/win.cw); x++)
					term.line[term.c.y][x].mode |= ATTR_SIXEL;
				tnewline(1);
			}
		}
		#endif // SIXEL_PATCH
		#if SYNC_PATCH
		/* https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec */
		if (strstr(strescseq.buf, "=1s") == strescseq.buf)
			tsync_begin();  /* BSU */
		else if (strstr(strescseq.buf, "=2s") == strescseq.buf)
			tsync_end();  /* ESU */
		#endif // SYNC_PATCH
		#if SIXEL_PATCH || SYNC_PATCH
		return;
		#endif // SIXEL_PATCH | SYNC_PATCH
	case '_': /* APC -- Application Program Command */
	case '^': /* PM -- Privacy Message */
		return;
	}

	fprintf(stderr, "erresc: unknown str ");
	strdump();
}

void
strparse(void)
{
	int c;
	char *p = strescseq.buf;

	strescseq.narg = 0;
	strescseq.buf[strescseq.len] = '\0';

	if (*p == '\0')
		return;

	while (strescseq.narg < STR_ARG_SIZ) {
		strescseq.args[strescseq.narg++] = p;
		while ((c = *p) != ';' && c != '\0')
			++p;
		if (c == '\0')
			return;
		*p++ = '\0';
	}
}

void
strdump(void)
{
	size_t i;
	uint c;

	fprintf(stderr, "ESC%c", strescseq.type);
	for (i = 0; i < strescseq.len; i++) {
		c = strescseq.buf[i] & 0xff;
		if (c == '\0') {
			putc('\n', stderr);
			return;
		} else if (isprint(c)) {
			putc(c, stderr);
		} else if (c == '\n') {
			fprintf(stderr, "(\\n)");
		} else if (c == '\r') {
			fprintf(stderr, "(\\r)");
		} else if (c == 0x1b) {
			fprintf(stderr, "(\\e)");
		} else {
			fprintf(stderr, "(%02x)", c);
		}
	}
	fprintf(stderr, "ESC\\\n");
}

void
strreset(void)
{
	strescseq = (STREscape){
		.buf = xrealloc(strescseq.buf, STR_BUF_SIZ),
		.siz = STR_BUF_SIZ,
	};
}

void
sendbreak(const Arg *arg)
{
	if (tcsendbreak(cmdfd, 0))
		perror("Error sending break");
}

void
tprinter(char *s, size_t len)
{
	if (iofd != -1 && xwrite(iofd, s, len) < 0) {
		perror("Error writing to output file");
		close(iofd);
		iofd = -1;
	}
}

void
toggleprinter(const Arg *arg)
{
	term.mode ^= MODE_PRINT;
}

void
printscreen(const Arg *arg)
{
	tdump();
}

void
printsel(const Arg *arg)
{
	tdumpsel();
}

void
tdumpsel(void)
{
	char *ptr;

	if ((ptr = getsel())) {
		tprinter(ptr, strlen(ptr));
		free(ptr);
	}
}

void
tdumpline(int n)
{
	char buf[UTF_SIZ];
	const Glyph *bp, *end;

	bp = &term.line[n][0];
	end = &bp[MIN(tlinelen(n), term.col) - 1];
	if (bp != end || bp->u != ' ') {
		for ( ; bp <= end; ++bp)
			tprinter(buf, utf8encode(bp->u, buf));
	}
	tprinter("\n", 1);
}

void
tdump(void)
{
	int i;

	for (i = 0; i < term.row; ++i)
		tdumpline(i);
}

void
tputtab(int n)
{
	uint x = term.c.x;

	if (n > 0) {
		while (x < term.col && n--)
			for (++x; x < term.col && !term.tabs[x]; ++x)
				/* nothing */ ;
	} else if (n < 0) {
		while (x > 0 && n++)
			for (--x; x > 0 && !term.tabs[x]; --x)
				/* nothing */ ;
	}
	term.c.x = LIMIT(x, 0, term.col-1);
}

void
tdefutf8(char ascii)
{
	if (ascii == 'G')
		term.mode |= MODE_UTF8;
	else if (ascii == '@')
		term.mode &= ~MODE_UTF8;
}

void
tdeftran(char ascii)
{
	static char cs[] = "0B";
	static int vcs[] = {CS_GRAPHIC0, CS_USA};
	char *p;

	if ((p = strchr(cs, ascii)) == NULL) {
		fprintf(stderr, "esc unhandled charset: ESC ( %c\n", ascii);
	} else {
		term.trantbl[term.icharset] = vcs[p - cs];
	}
}

void
tdectest(char c)
{
	int x, y;

	if (c == '8') { /* DEC screen alignment test. */
		for (x = 0; x < term.col; ++x) {
			for (y = 0; y < term.row; ++y)
				tsetchar('E', &term.c.attr, x, y);
		}
	}
}

void
tstrsequence(uchar c)
{
	#if SIXEL_PATCH
	strreset();
	#endif // SIXEL_PATCH

	switch (c) {
	case 0x90:   /* DCS -- Device Control String */
		c = 'P';
		#if SIXEL_PATCH
		term.esc |= ESC_DCS;
		#endif // SIXEL_PATCH
		break;
	case 0x9f:   /* APC -- Application Program Command */
		c = '_';
		break;
	case 0x9e:   /* PM -- Privacy Message */
		c = '^';
		break;
	case 0x9d:   /* OSC -- Operating System Command */
		c = ']';
		break;
	}
	#if !SIXEL_PATCH
	strreset();
	#endif // SIXEL_PATCH
	strescseq.type = c;
	term.esc |= ESC_STR;
}

void
tcontrolcode(uchar ascii)
{
	switch (ascii) {
	case '\t':   /* HT */
		tputtab(1);
		return;
	case '\b':   /* BS */
		tmoveto(term.c.x-1, term.c.y);
		return;
	case '\r':   /* CR */
		tmoveto(0, term.c.y);
		return;
	case '\f':   /* LF */
	case '\v':   /* VT */
	case '\n':   /* LF */
		/* go to first col if the mode is set */
		tnewline(IS_SET(MODE_CRLF));
		return;
	case '\a':   /* BEL */
		if (term.esc & ESC_STR_END) {
			/* backwards compatibility to xterm */
			strhandle();
		} else {
			xbell();
		}
		break;
	case '\033': /* ESC */
		csireset();
		term.esc &= ~(ESC_CSI|ESC_ALTCHARSET|ESC_TEST);
		term.esc |= ESC_START;
		return;
	case '\016': /* SO (LS1 -- Locking shift 1) */
	case '\017': /* SI (LS0 -- Locking shift 0) */
		term.charset = 1 - (ascii - '\016');
		return;
	case '\032': /* SUB */
		tsetchar('?', &term.c.attr, term.c.x, term.c.y);
		/* FALLTHROUGH */
	case '\030': /* CAN */
		csireset();
		break;
	case '\005': /* ENQ (IGNORED) */
	case '\000': /* NUL (IGNORED) */
	case '\021': /* XON (IGNORED) */
	case '\023': /* XOFF (IGNORED) */
	case 0177:   /* DEL (IGNORED) */
		return;
	case 0x80:   /* TODO: PAD */
	case 0x81:   /* TODO: HOP */
	case 0x82:   /* TODO: BPH */
	case 0x83:   /* TODO: NBH */
	case 0x84:   /* TODO: IND */
		break;
	case 0x85:   /* NEL -- Next line */
		tnewline(1); /* always go to first col */
		break;
	case 0x86:   /* TODO: SSA */
	case 0x87:   /* TODO: ESA */
		break;
	case 0x88:   /* HTS -- Horizontal tab stop */
		term.tabs[term.c.x] = 1;
		break;
	case 0x89:   /* TODO: HTJ */
	case 0x8a:   /* TODO: VTS */
	case 0x8b:   /* TODO: PLD */
	case 0x8c:   /* TODO: PLU */
	case 0x8d:   /* TODO: RI */
	case 0x8e:   /* TODO: SS2 */
	case 0x8f:   /* TODO: SS3 */
	case 0x91:   /* TODO: PU1 */
	case 0x92:   /* TODO: PU2 */
	case 0x93:   /* TODO: STS */
	case 0x94:   /* TODO: CCH */
	case 0x95:   /* TODO: MW */
	case 0x96:   /* TODO: SPA */
	case 0x97:   /* TODO: EPA */
	case 0x98:   /* TODO: SOS */
	case 0x99:   /* TODO: SGCI */
		break;
	case 0x9a:   /* DECID -- Identify Terminal */
		ttywrite(vtiden, strlen(vtiden), 0);
		break;
	case 0x9b:   /* TODO: CSI */
	case 0x9c:   /* TODO: ST */
		break;
	case 0x90:   /* DCS -- Device Control String */
	case 0x9d:   /* OSC -- Operating System Command */
	case 0x9e:   /* PM -- Privacy Message */
	case 0x9f:   /* APC -- Application Program Command */
		tstrsequence(ascii);
		return;
	}
	/* only CAN, SUB, \a and C1 chars interrupt a sequence */
	term.esc &= ~(ESC_STR_END|ESC_STR);
}

/*
 * returns 1 when the sequence is finished and it hasn't to read
 * more characters for this sequence, otherwise 0
 */
int
eschandle(uchar ascii)
{
	switch (ascii) {
	case '[':
		term.esc |= ESC_CSI;
		return 0;
	case '#':
		term.esc |= ESC_TEST;
		return 0;
	case '%':
		term.esc |= ESC_UTF8;
		return 0;
	case 'P': /* DCS -- Device Control String */
		#if SIXEL_PATCH
		term.esc |= ESC_DCS;
		#endif // SIXEL_PATCH
	case '_': /* APC -- Application Program Command */
	case '^': /* PM -- Privacy Message */
	case ']': /* OSC -- Operating System Command */
	case 'k': /* old title set compatibility */
		tstrsequence(ascii);
		return 0;
	case 'n': /* LS2 -- Locking shift 2 */
	case 'o': /* LS3 -- Locking shift 3 */
		term.charset = 2 + (ascii - 'n');
		break;
	case '(': /* GZD4 -- set primary charset G0 */
	case ')': /* G1D4 -- set secondary charset G1 */
	case '*': /* G2D4 -- set tertiary charset G2 */
	case '+': /* G3D4 -- set quaternary charset G3 */
		term.icharset = ascii - '(';
		term.esc |= ESC_ALTCHARSET;
		return 0;
	case 'D': /* IND -- Linefeed */
		if (term.c.y == term.bot) {
			#if SCROLLBACK_PATCH
			tscrollup(term.top, 1, 1);
			#else
			tscrollup(term.top, 1);
			#endif // SCROLLBACK_PATCH
		} else {
			tmoveto(term.c.x, term.c.y+1);
		}
		break;
	case 'E': /* NEL -- Next line */
		tnewline(1); /* always go to first col */
		break;
	case 'H': /* HTS -- Horizontal tab stop */
		term.tabs[term.c.x] = 1;
		break;
	case 'M': /* RI -- Reverse index */
		if (term.c.y == term.top) {
			#if SCROLLBACK_PATCH
			tscrolldown(term.top, 1, 1);
			#else
			tscrolldown(term.top, 1);
			#endif // SCROLLBACK_PATCH
		} else {
			tmoveto(term.c.x, term.c.y-1);
		}
		break;
	case 'Z': /* DECID -- Identify Terminal */
		ttywrite(vtiden, strlen(vtiden), 0);
		break;
	case 'c': /* RIS -- Reset to initial state */
		treset();
		#if CSI_22_23_PATCH
		xfreetitlestack();
		#endif // CSI_22_23_PATCH
		resettitle();
		xloadcols();
		break;
	case '=': /* DECPAM -- Application keypad */
		xsetmode(1, MODE_APPKEYPAD);
		break;
	case '>': /* DECPNM -- Normal keypad */
		xsetmode(0, MODE_APPKEYPAD);
		break;
	case '7': /* DECSC -- Save Cursor */
		tcursor(CURSOR_SAVE);
		break;
	case '8': /* DECRC -- Restore Cursor */
		tcursor(CURSOR_LOAD);
		break;
	case '\\': /* ST -- String Terminator */
		if (term.esc & ESC_STR_END)
			strhandle();
		break;
	default:
		fprintf(stderr, "erresc: unknown sequence ESC 0x%02X '%c'\n",
			(uchar) ascii, isprint(ascii)? ascii:'.');
		break;
	}
	return 1;
}

void
tputc(Rune u)
{
	char c[UTF_SIZ];
	int control;
	int width, len;
	Glyph *gp;

	control = ISCONTROL(u);
	#if SIXEL_PATCH
	if (u < 127 || !IS_SET(MODE_UTF8 | MODE_SIXEL))
	#else
	if (u < 127 || !IS_SET(MODE_UTF8))
	#endif // SIXEL_PATCH
	{
		c[0] = u;
		width = len = 1;
	} else {
		len = utf8encode(u, c);
		if (!control && (width = wcwidth(u)) == -1)
			width = 1;
	}

	if (IS_SET(MODE_PRINT))
		tprinter(c, len);

	/*
	 * STR sequence must be checked before anything else
	 * because it uses all following characters until it
	 * receives a ESC, a SUB, a ST or any other C1 control
	 * character.
	 */
	if (term.esc & ESC_STR) {
		if (u == '\a' || u == 030 || u == 032 || u == 033 ||
		   ISCONTROLC1(u)) {
			#if SIXEL_PATCH
			term.esc &= ~(ESC_START|ESC_STR|ESC_DCS);
			#else
			term.esc &= ~(ESC_START|ESC_STR);
			#endif // SIXEL_PATCH
			term.esc |= ESC_STR_END;
			goto check_control_code;
		}

		#if SIXEL_PATCH
		if (IS_SET(MODE_SIXEL)) {
			if (sixel_parser_parse(&sixel_st, (unsigned char *)&u, 1) != 0)
				perror("sixel_parser_parse() failed");
			return;
		}
		if (term.esc & ESC_DCS)
			goto check_control_code;
		#endif // SIXEL_PATCH

		if (strescseq.len+len >= strescseq.siz) {
			/*
			 * Here is a bug in terminals. If the user never sends
			 * some code to stop the str or esc command, then st
			 * will stop responding. But this is better than
			 * silently failing with unknown characters. At least
			 * then users will report back.
			 *
			 * In the case users ever get fixed, here is the code:
			 */
			/*
			 * term.esc = 0;
			 * strhandle();
			 */
			if (strescseq.siz > (SIZE_MAX - UTF_SIZ) / 2)
				return;
			strescseq.siz *= 2;
			strescseq.buf = xrealloc(strescseq.buf, strescseq.siz);
		}

		memmove(&strescseq.buf[strescseq.len], c, len);
		strescseq.len += len;
		return;
	}

check_control_code:
	/*
	 * Actions of control codes must be performed as soon they arrive
	 * because they can be embedded inside a control sequence, and
	 * they must not cause conflicts with sequences.
	 */
	if (control) {
		tcontrolcode(u);
		/*
		 * control codes are not shown ever
		 */
		if (!term.esc)
			term.lastc = 0;
		return;
	} else if (term.esc & ESC_START) {
		if (term.esc & ESC_CSI) {
			csiescseq.buf[csiescseq.len++] = u;
			if (BETWEEN(u, 0x40, 0x7E)
					|| csiescseq.len >= \
					sizeof(csiescseq.buf)-1) {
				term.esc = 0;
				csiparse();
				csihandle();
			}
			return;
		#if SIXEL_PATCH
		} else if (term.esc & ESC_DCS) {
			csiescseq.buf[csiescseq.len++] = u;
			if (BETWEEN(u, 0x40, 0x7E)
					|| csiescseq.len >= \
					sizeof(csiescseq.buf)-1) {
				csiparse();
				dcshandle();
			}
			return;
		#endif // SIXEL_PATCH
		} else if (term.esc & ESC_UTF8) {
			tdefutf8(u);
		} else if (term.esc & ESC_ALTCHARSET) {
			tdeftran(u);
		} else if (term.esc & ESC_TEST) {
			tdectest(u);
		} else {
			if (!eschandle(u))
				return;
			/* sequence already finished */
		}
		term.esc = 0;
		/*
		 * All characters which form part of a sequence are not
		 * printed
		 */
		return;
	}
	#if !VIM_BROWSE_PATCH
	if (selected(term.c.x, term.c.y))
		selclear();
	#endif // VIM_BROWSE_PATCH

	gp = &term.line[term.c.y][term.c.x];
	if (IS_SET(MODE_WRAP) && (term.c.state & CURSOR_WRAPNEXT)) {
		gp->mode |= ATTR_WRAP;
		tnewline(1);
		gp = &term.line[term.c.y][term.c.x];
	}

	if (IS_SET(MODE_INSERT) && term.c.x+width < term.col)
		memmove(gp+width, gp, (term.col - term.c.x - width) * sizeof(Glyph));

	if (term.c.x+width > term.col) {
		tnewline(1);
		gp = &term.line[term.c.y][term.c.x];
	}

	tsetchar(u, &term.c.attr, term.c.x, term.c.y);
	term.lastc = u;

	if (width == 2) {
		gp->mode |= ATTR_WIDE;
		if (term.c.x+1 < term.col) {
			gp[1].u = '\0';
			gp[1].mode = ATTR_WDUMMY;
		}
	}
	if (term.c.x+width < term.col) {
		tmoveto(term.c.x+width, term.c.y);
	} else {
		term.c.state |= CURSOR_WRAPNEXT;
	}
}

int
twrite(const char *buf, int buflen, int show_ctrl)
{
	int charsize;
	Rune u;
	int n;

	#if SYNC_PATCH
	int su0 = su;
	twrite_aborted = 0;
	#endif // SYNC_PATCH

	for (n = 0; n < buflen; n += charsize) {
		#if SIXEL_PATCH
		if (IS_SET(MODE_UTF8) && !IS_SET(MODE_SIXEL))
		#else
		if (IS_SET(MODE_UTF8))
		#endif // SIXEL_PATCH
		{
			/* process a complete utf8 char */
			charsize = utf8decode(buf + n, &u, buflen - n);
			if (charsize == 0)
				break;
		} else {
			u = buf[n] & 0xFF;
			charsize = 1;
		}
		#if SYNC_PATCH
		if (su0 && !su) {
			twrite_aborted = 1;
			break;  // ESU - allow rendering before a new BSU
		}
		#endif // SYNC_PATCH
		if (show_ctrl && ISCONTROL(u)) {
			if (u & 0x80) {
				u &= 0x7f;
				tputc('^');
				tputc('[');
			} else if (u != '\n' && u != '\r' && u != '\t') {
				u ^= 0x40;
				tputc('^');
			}
		}
		tputc(u);
	}
	return n;
}

void
tresize(int col, int row)
{
	int i;
	#if SCROLLBACK_PATCH
	int j;
	#endif // SCROLLBACK_PATCH
	#if VIM_BROWSE_PATCH
	int const colSet = col, alt = IS_SET(MODE_ALTSCREEN), ini = buf == NULL;
	col = MAX(col, buffCols);
	row = MIN(row, buffSize);
	int const minrow = MIN(row, term.row), mincol = MIN(col, buffCols);
	#elif COLUMNS_PATCH
	int tmp = col;
	int minrow, mincol;

	if (!term.maxcol)
		term.maxcol = term.col;
	col = MAX(col, term.maxcol);
	minrow = MIN(row, term.row);
	mincol = MIN(col, term.maxcol);
	#else
	int minrow = MIN(row, term.row);
	int mincol = MIN(col, term.col);
	#endif // VIM_BROWSE_PATCH
	int *bp;
	TCursor c;

	#if KEYBOARDSELECT_PATCH
	if ( row < term.row  || col < term.col )
		toggle_winmode(trt_kbdselect(XK_Escape, NULL, 0));
	#endif // KEYBOARDSELECT_PATCH

	if (col < 1 || row < 1) {
		fprintf(stderr,
		        "tresize: error resizing to %dx%d\n", col, row);
		return;
	}

	#if VIM_BROWSE_PATCH
	if (alt)
		tswapscreen();
	#endif // VIM_BROWSE_PATCH

	/*
	 * slide screen to keep cursor where we expect it -
	 * tscrollup would work here, but we can optimize to
	 * memmove because we're freeing the earlier lines
	 */
	for (i = 0; i <= term.c.y - row; i++) {
		#if !VIM_BROWSE_PATCH
		free(term.line[i]);
		#endif // VIM_BROWSE_PATCH
		free(term.alt[i]);
	}
	/* ensure that both src and dst are not NULL */
	if (i > 0) {
		#if !VIM_BROWSE_PATCH
		memmove(term.line, term.line + i, row * sizeof(Line));
		#endif // VIM_BROWSE_PATCH
		memmove(term.alt, term.alt + i, row * sizeof(Line));
	}
	for (i += row; i < term.row; i++) {
		#if !VIM_BROWSE_PATCH
		free(term.line[i]);
		#endif // VIM_BROWSE_PATCH
		free(term.alt[i]);
	}

	/* resize to new height */
	#if VIM_BROWSE_PATCH
	buf = xrealloc(buf, (buffSize + row) * sizeof(Line));
	mark = xrealloc(mark, col * row * sizeof(*mark));
	#else
	term.line = xrealloc(term.line, row * sizeof(Line));
	#endif // VIM_BROWSE_PATCH
	term.alt  = xrealloc(term.alt,  row * sizeof(Line));
	term.dirty = xrealloc(term.dirty, row * sizeof(*term.dirty));
	term.tabs = xrealloc(term.tabs, col * sizeof(*term.tabs));

	#if SCROLLBACK_PATCH
	for (i = 0; i < HISTSIZE; i++) {
		term.hist[i] = xrealloc(term.hist[i], col * sizeof(Glyph));
		for (j = mincol; j < col; j++) {
			term.hist[i][j] = term.c.attr;
			term.hist[i][j].u = ' ';
		}
	}
	#endif // SCROLLBACK_PATCH

	/* resize each row to new width, zero-pad if needed */
	for (i = 0; i < minrow; i++) {
		#if !VIM_BROWSE_PATCH
		term.line[i] = xrealloc(term.line[i], col * sizeof(Glyph));
		#endif // VIM_BROWSE_PATCH
		term.alt[i]  = xrealloc(term.alt[i],  col * sizeof(Glyph));
	}

	/* allocate any new rows */
	for (/* i = minrow */; i < row; i++) {
		#if !VIM_BROWSE_PATCH
		term.line[i] = xmalloc(col * sizeof(Glyph));
		#endif // VIM_BROWSE_PATCH
		term.alt[i] = xmalloc(col * sizeof(Glyph));
	}
	#if VIM_BROWSE_PATCH
	if (col > buffCols)
	#elif COLUMNS_PATCH
	if (col > term.maxcol)
	#else
	if (col > term.col)
	#endif // VIM_BROWSE_PATCH
	{
		#if VIM_BROWSE_PATCH
		bp = term.tabs + buffCols;
		memset(bp, 0, sizeof(*term.tabs) * (col - buffCols));
		#elif COLUMNS_PATCH
		bp = term.tabs + term.maxcol;
		memset(bp, 0, sizeof(*term.tabs) * (col - term.maxcol));
		#else
		bp = term.tabs + term.col;
		memset(bp, 0, sizeof(*term.tabs) * (col - term.col));
		#endif // VIM_BROWSE_PATCH

		while (--bp > term.tabs && !*bp)
			/* nothing */ ;
		for (bp += tabspaces; bp < term.tabs + col; bp += tabspaces)
			*bp = 1;
	}
	#if VIM_BROWSE_PATCH
	Glyph g=(Glyph){.bg=term.c.attr.bg, .fg=term.c.attr.fg, .u=' ', .mode=0};
	for (i = 0; i < buffSize; ++i) {
		buf[i] = xrealloc(ini ? NULL : buf[i], col*sizeof(Glyph));
		for (int j = ini ? 0 : buffCols; j < col; ++j) buf[i][j] = g;
	}
	for (i = 0; i < row; ++i) buf[buffSize + i] = buf[i];
	term.line = &buf[*(histOp?&histOff:&insertOff) +=MAX(term.c.y-row+1,0)];
	memset(mark, 0, col * row * sizeof(*mark));
	#endif // VIM_BROWSE_PATCH
	/* update terminal size */
	#if VIM_BROWSE_PATCH
	term.col = colSet;
	buffCols = col;
	#elif COLUMNS_PATCH
	term.col = tmp;
	term.maxcol = col;
	#else
	term.col = col;
	#endif // VIM_BROWSE_PATCH
	term.row = row;
	#if VIM_BROWSE_PATCH
	if (alt)
		tswapscreen();
	#endif // VIM_BROWSE_PATCH
	/* reset scrolling region */
	tsetscroll(0, row-1);
	/* make use of the LIMIT in tmoveto */
	tmoveto(term.c.x, term.c.y);
	/* Clearing both screens (it makes dirty all lines) */
	c = term.c;
	for (i = 0; i < 2; i++) {
		if (mincol < col && 0 < minrow) {
			tclearregion(mincol, 0, col - 1, minrow - 1);
		}
		if (0 < col && minrow < row) {
			tclearregion(0, minrow, col - 1, row - 1);
		}
		tswapscreen();
		tcursor(CURSOR_LOAD);
	}
	term.c = c;
}

void
resettitle(void)
{
	#if CSI_22_23_PATCH
	xsettitle(NULL, 0);
	#else
	xsettitle(NULL);
	#endif // CSI_22_23_PATCH
}

void
drawregion(int x1, int y1, int x2, int y2)
{
	#if VIM_BROWSE_PATCH
	if (altToggle && histMode && !histOp)
		memset(term.dirty, 0, sizeof(*term.dirty) * term.row);
	int const o = !IS_SET(MODE_ALTSCREEN) && histMode && !histOp, h =rows();
	#endif // VIM_BROWSE_PATCH
	int y;

	for (y = y1; y < y2; y++) {
		#if VIM_BROWSE_PATCH
		int const oy = o ? (y + insertOff - histOff + h) % h : y;
		if (!BETWEEN(oy, 0, term.row-1) || !term.dirty[y]) continue;
		xdrawline(term.line[y], x1, oy, x2);
		#else
		if (!term.dirty[y])
			continue;

		term.dirty[y] = 0;
		#if SCROLLBACK_PATCH
		xdrawline(TLINE(y), x1, y, x2);
		#else
		xdrawline(term.line[y], x1, y, x2);
		#endif // SCROLLBACK_PATCH
		#endif // VIM_BROWSE_PATCH
	}
	#if VIM_BROWSE_PATCH
	memset(&term.dirty[y1], 0, sizeof(*term.dirty) * (y2 - y1));
	#endif // VIM_BROWSE_PATCH
}

#include "patch/st_include.c"

void
draw(void)
{
	int cx = term.c.x, ocx = term.ocx, ocy = term.ocy;

	if (!xstartdraw())
		return;

	/* adjust cursor position */
	LIMIT(term.ocx, 0, term.col-1);
	LIMIT(term.ocy, 0, term.row-1);
	if (term.line[term.ocy][term.ocx].mode & ATTR_WDUMMY)
		term.ocx--;
	if (term.line[term.c.y][cx].mode & ATTR_WDUMMY)
		cx--;

	#if VIM_BROWSE_PATCH
	if (histMode)
		historyPreDraw();
	#endif // VIM_BROWSE_PATCH

	drawregion(0, 0, term.col, term.row);

	#if VIM_BROWSE_PATCH
	if (!histMode)
	#elif SCROLLBACK_PATCH
	if (term.scr == 0)
	#endif // SCROLLBACK_PATCH
	#if LIGATURES_PATCH
	xdrawcursor(cx, term.c.y, term.line[term.c.y][cx],
			term.ocx, term.ocy, term.line[term.ocy][term.ocx],
			term.line[term.ocy], term.col);
	#else
	xdrawcursor(cx, term.c.y, term.line[term.c.y][cx],
			term.ocx, term.ocy, term.line[term.ocy][term.ocx]);
	#endif // LIGATURES_PATCH
	term.ocx = cx;
	term.ocy = term.c.y;
	xfinishdraw();
	if (ocx != term.ocx || ocy != term.ocy)
		xximspot(term.ocx, term.ocy);
}

void
redraw(void)
{
	tfulldirt();
	draw();
}
