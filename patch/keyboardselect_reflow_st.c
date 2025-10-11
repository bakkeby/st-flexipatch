#include <wctype.h>

enum keyboardselect_mode {
	KBDS_MODE_MOVE    = 0,
	KBDS_MODE_SELECT  = 1<<1,
	KBDS_MODE_LSELECT = 1<<2,
	KBDS_MODE_FIND    = 1<<3,
	KBDS_MODE_SEARCH  = 1<<4,
};

enum cursor_wrap {
	KBDS_WRAP_NONE    = 0,
	KBDS_WRAP_LINE    = 1<<0,
	KBDS_WRAP_EDGE    = 1<<1,
};

typedef struct {
	int x;
	int y;
	Line line;
	int len;
} KCursor;

static int kbds_in_use, kbds_quant;
static int kbds_seltype = SEL_REGULAR;
static int kbds_mode, kbds_directsearch;
static int kbds_searchlen, kbds_searchdir, kbds_searchcase;
static int kbds_finddir, kbds_findtill;
static Glyph *kbds_searchstr;
static Rune kbds_findchar;
static KCursor kbds_c, kbds_oc;

void
kbds_drawstatusbar(int y)
{
	static char *modes[] = { " MOVE ", "", " SELECT ", " RSELECT ", " LSELECT ",
	                         " SEARCH FW ", " SEARCH BW ", " FIND FW ", " FIND BW " };
	static char quant[20] = { ' ' };
	static Glyph g;
	int i, n, m;
	int mlen, qlen;

	if (!kbds_in_use)
		return;

	g.mode = ATTR_REVERSE;
	g.fg = defaultfg;
	g.bg = defaultbg;

	if (y == 0) {
		if (kbds_issearchmode())
			m = 5 + (kbds_searchdir < 0 ? 1 : 0);
		else if (kbds_mode & KBDS_MODE_FIND)
			m = 7 + (kbds_finddir < 0 ? 1 : 0);
		else if (kbds_mode & KBDS_MODE_SELECT)
			m = 2 + (kbds_seltype == SEL_RECTANGULAR ? 1 : 0);
		else
			m = kbds_mode;
		mlen = strlen(modes[m]);
		qlen = kbds_quant ? snprintf(quant+1, sizeof quant-1, "%i", kbds_quant) + 1 : 0;
		if (kbds_c.y != y || kbds_c.x < term.col - qlen - mlen) {
			for (n = mlen, i = term.col-1; i >= 0 && n > 0; i--) {
				g.u = modes[m][--n];
				xdrawglyph(g, i, y);
			}
			for (n = qlen; i >= 0 && n > 0; i--) {
				g.u = quant[--n];
				xdrawglyph(g, i, y);
			}
		}
	}

	if (y == term.row-1 && kbds_issearchmode()) {
		for (g.u = ' ', i = 0; i < term.col; i++)
			xdrawglyph(g, i, y);
		g.u = (kbds_searchdir > 0) ? '/' : '?';
		xdrawglyph(g, 0, y);
		for (i = 0; i < kbds_searchlen; i++) {
			g.u = kbds_searchstr[i].u;
			g.mode = kbds_searchstr[i].mode | ATTR_WIDE | ATTR_REVERSE;
			if (g.u == ' ' || g.mode & ATTR_WDUMMY)
				continue;
			xdrawglyph(g, i + 1, y);
		}
		g.u = ' ';
		g.mode = ATTR_NULL;
		xdrawglyph(g, i + 1, y);
	}
}

void
kbds_pasteintosearch(const char *data, int len, int append)
{
	static char buf[BUFSIZ];
	static int buflen;
	Rune u;
	int l, n, charsize;

	if (!append)
		buflen = 0;

	for (; len > 0; len -= l, data += l) {
		l = MIN(sizeof(buf) - buflen, len);
		memmove(buf + buflen, data, l);
		buflen += l;
		for (n = 0; n < buflen; n += charsize) {
			if (IS_SET(MODE_UTF8)) {
				/* process a complete utf8 char */
				charsize = utf8decode(buf + n, &u, buflen - n);
				if (charsize == 0)
					break;
			} else {
				u = buf[n] & 0xFF;
				charsize = 1;
			}
			if (u > 0x1f && kbds_searchlen < term.col-2) {
				kbds_searchstr[kbds_searchlen].u = u;
				kbds_searchstr[kbds_searchlen++].mode = ATTR_NULL;
				if (wcwidth(u) > 1) {
					kbds_searchstr[kbds_searchlen-1].mode = ATTR_WIDE;
					if (kbds_searchlen < term.col-2) {
						kbds_searchstr[kbds_searchlen].u = 0;
						kbds_searchstr[kbds_searchlen++].mode = ATTR_WDUMMY;
					}
				}
			}
		}
		buflen -= n;
		/* keep any incomplete UTF-8 byte sequence for the next call */
		if (buflen > 0)
			memmove(buf, buf + n, buflen);
	}
	term.dirty[term.row-1] = 1;
}

int
kbds_top(void)
{
	return IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf + term.scr;
}

int
kbds_bot(void)
{
	return IS_SET(MODE_ALTSCREEN) ? term.row-1 : term.row-1 + term.scr;
}

int
kbds_iswrapped(KCursor *c)
{
    return c->len > 0 && (c->line[c->len-1].mode & ATTR_WRAP);
}

int
kbds_isselectmode(void)
{
	return kbds_in_use && (kbds_mode & (KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
}

int
kbds_issearchmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_SEARCH);
}

void
kbds_setmode(int mode)
{
	kbds_mode = mode;
	term.dirty[0] = 1;
}

void
kbds_selecttext(void)
{
	if (kbds_isselectmode()) {
		if (kbds_mode & KBDS_MODE_LSELECT)
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
		else
			selextend(kbds_c.x, kbds_c.y, kbds_seltype, 0);
		if (sel.mode == SEL_IDLE)
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
	}
}

void
kbds_copytoclipboard(void)
{
	if (kbds_mode & KBDS_MODE_LSELECT) {
		selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 1);
		sel.type = SEL_REGULAR;
	} else {
		selextend(kbds_c.x, kbds_c.y, kbds_seltype, 1);
	}
	xsetsel(getsel());

	#if !CLIPBOARD_PATCH
	xclipcopy();
	#endif // CLIPBOARD_PATCH
}

void
kbds_clearhighlights(void)
{
	int x, y;
	Line line;

	for (y = (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf); y < term.row; y++) {
		line = TLINEABS(y);
		for (x = 0; x < term.col; x++)
			line[x].mode &= ~ATTR_HIGHLIGHT;
	}
	tfulldirt();
}

int
kbds_moveto(int x, int y)
{
	if (y < 0)
		kscrollup(&((Arg){ .i = -y }));
	else if (y >= term.row)
		kscrolldown(&((Arg){ .i = y - term.row + 1 }));
	kbds_c.x = (x < 0) ? 0 : (x > term.col-1) ? term.col-1 : x;
	kbds_c.y = (y < 0) ? 0 : (y > term.row-1) ? term.row-1 : y;
	kbds_c.line = TLINE(kbds_c.y);
	kbds_c.len = tlinelen(kbds_c.line);
	if (kbds_c.x > 0 && (kbds_c.line[kbds_c.x].mode & ATTR_WDUMMY))
		kbds_c.x--;
}

int
kbds_moveforward(KCursor *c, int dx, int wrap)
{
	KCursor n = *c;

	n.x += dx;
	if (n.x >= 0 && n.x < term.col && (n.line[n.x].mode & ATTR_WDUMMY))
		n.x += dx;

	if (n.x < 0) {
		if (!wrap || --n.y < kbds_top())
			return 0;
		n.line = TLINE(n.y);
		n.len = tlinelen(n.line);
		if ((wrap & KBDS_WRAP_LINE) && kbds_iswrapped(&n))
			n.x = n.len-1;
		else if (wrap & KBDS_WRAP_EDGE)
			n.x = term.col-1;
		else
			return 0;
		n.x -= (n.x > 0 && (n.line[n.x].mode & ATTR_WDUMMY)) ? 1 : 0;
	} else if (n.x >= term.col) {
		if (((wrap & KBDS_WRAP_EDGE) ||
		    ((wrap & KBDS_WRAP_LINE) && kbds_iswrapped(&n))) && ++n.y <= kbds_bot()) {
			n.line = TLINE(n.y);
			n.len = tlinelen(n.line);
			n.x = 0;
		} else {
			return 0;
		}
	} else if (n.x >= n.len && dx > 0 && (wrap & KBDS_WRAP_LINE)) {
		if (n.x == n.len && kbds_iswrapped(&n) && n.y < kbds_bot()) {
			++n.y;
			n.line = TLINE(n.y);
			n.len = tlinelen(n.line);
			n.x = 0;
		} else if (!(wrap & KBDS_WRAP_EDGE)) {
			return 0;
		}
	}
	*c = n;
	return 1;
}

int
kbds_ismatch(KCursor c)
{
	KCursor m = c;
	int i, next;

	if (c.x + kbds_searchlen > c.len && (!kbds_iswrapped(&c) || c.y >= kbds_bot()))
		return 0;

	for (next = 0, i = 0; i < kbds_searchlen; i++) {
		if (kbds_searchstr[i].mode & ATTR_WDUMMY)
			continue;
		if ((next++ && !kbds_moveforward(&c, 1, KBDS_WRAP_LINE)) ||
		    (kbds_searchcase && kbds_searchstr[i].u != c.line[c.x].u) ||
		    (!kbds_searchcase && kbds_searchstr[i].u != towlower(c.line[c.x].u)))
			return 0;
	}

	for (i = 0; i < kbds_searchlen; i++) {
		if (!(kbds_searchstr[i].mode & ATTR_WDUMMY)) {
			m.line[m.x].mode |= ATTR_HIGHLIGHT;
			kbds_moveforward(&m, 1, KBDS_WRAP_LINE);
		}
	}
	return 1;
}

int
kbds_searchall(void)
{
	KCursor c;
	int count = 0;

	if (!kbds_searchlen)
		return 0;

	for (c.y = kbds_top(); c.y <= kbds_bot(); c.y++) {
		c.line = TLINE(c.y);
		c.len = tlinelen(c.line);
		for (c.x = 0; c.x < c.len; c.x++)
			count += kbds_ismatch(c);
	}
	tfulldirt();

	return count;
}

void
kbds_searchnext(int dir)
{
	KCursor c = kbds_c, n = kbds_c;
	int wrapped = 0;

	if (!kbds_searchlen) {
		kbds_quant = 0;
		return;
	}

	if (dir < 0 && c.x > c.len)
		c.x = c.len;

	for (kbds_quant = MAX(kbds_quant, 1); kbds_quant > 0;) {
		if (!kbds_moveforward(&c, dir, KBDS_WRAP_LINE)) {
			c.y += dir;
			if (c.y < kbds_top())
				c.y = kbds_bot(), wrapped++;
			else if (c.y > kbds_bot())
				c.y = kbds_top(), wrapped++;
			if (wrapped > 1)
				break;;
			c.line = TLINE(c.y);
			c.len = tlinelen(c.line);
			c.x = (dir < 0 && c.len > 0) ? c.len-1 : 0;
			c.x -= (c.x > 0 && (c.line[c.x].mode & ATTR_WDUMMY)) ? 1 : 0;
		}
		if (kbds_ismatch(c)) {
			n = c;
			kbds_quant--;
		}
	}

	kbds_moveto(n.x, n.y);
	kbds_quant = 0;
}

void
kbds_findnext(int dir, int repeat)
{
	KCursor prev, c = kbds_c, n = kbds_c;
	int skipfirst, yoff = 0;

	if (c.len <= 0 || kbds_findchar == 0) {
		kbds_quant = 0;
		return;
	}

	if (dir < 0 && c.x > c.len)
		c.x = c.len;

	kbds_quant = MAX(kbds_quant, 1);
	skipfirst = (kbds_quant == 1 && repeat && kbds_findtill);

	while (kbds_quant > 0) {
		prev = c;
		if (!kbds_moveforward(&c, dir, KBDS_WRAP_LINE))
			break;
		if (c.line[c.x].u == kbds_findchar) {
			if (skipfirst && prev.x == kbds_c.x && prev.y == kbds_c.y) {
				skipfirst = 0;
				continue;
			}
			n.x = kbds_findtill ? prev.x : c.x;
			n.y = c.y;
			yoff = kbds_findtill ? prev.y - c.y : 0;
			kbds_quant--;
		}
	}

	kbds_moveto(n.x, n.y);
	kbds_moveto(kbds_c.x, kbds_c.y + yoff);
	kbds_quant = 0;
}

int
kbds_isdelim(KCursor c, int xoff, wchar_t *delims)
{
	if (xoff && !kbds_moveforward(&c, xoff, KBDS_WRAP_LINE))
		return 1;
	return wcschr(delims, c.line[c.x].u) != NULL;
}

void
kbds_nextword(int start, int dir, wchar_t *delims)
{
	KCursor c = kbds_c, n = kbds_c;
	int xoff = start ? -1 : 1;

	if (dir < 0 && c.x > c.len)
		c.x = c.len;
	else if (dir > 0 && c.x >= c.len && c.len > 0)
		c.x = c.len-1;

	for (kbds_quant = MAX(kbds_quant, 1); kbds_quant > 0;) {
		if (!kbds_moveforward(&c, dir, KBDS_WRAP_LINE)) {
			c.y += dir;
			if (c.y < kbds_top() || c.y > kbds_bot())
				break;
			c.line = TLINE(c.y);
			c.len = tlinelen(c.line);
			c.x = (dir < 0 && c.len > 0) ? c.len-1 : 0;
			c.x -= (c.x > 0 && (c.line[c.x].mode & ATTR_WDUMMY)) ? 1 : 0;
		}
		if (c.len > 0 &&
		    !kbds_isdelim(c, 0, delims) && kbds_isdelim(c, xoff, delims)) {
			n = c;
			kbds_quant--;
		}
	}

	kbds_moveto(n.x, n.y);
	kbds_quant = 0;
}

int
kbds_drawcursor(void)
{
	if (kbds_in_use && (!kbds_issearchmode() || kbds_c.y != term.row-1)) {
		#if LIGATURES_PATCH
		xdrawcursor(kbds_c.x, kbds_c.y, TLINE(kbds_c.y)[kbds_c.x],
					kbds_oc.x, kbds_oc.y, TLINE(kbds_oc.y)[kbds_oc.x],
					TLINE(kbds_oc.y), term.col);
		#else
		xdrawcursor(kbds_c.x, kbds_c.y, TLINE(kbds_c.y)[kbds_c.x],
					kbds_oc.x, kbds_oc.y, TLINE(kbds_oc.y)[kbds_oc.x]);
		#endif // LIGATURES_PATCH
		kbds_moveto(kbds_c.x, kbds_c.y);
		kbds_oc = kbds_c;
	}
	return term.scr != 0 || kbds_in_use;
}

int
kbds_keyboardhandler(KeySym ksym, char *buf, int len, int forcequit)
{
	int i, q, dy, eol, islast, prevscr, count, wrap;
	int alt = IS_SET(MODE_ALTSCREEN);
	Line line;
	Rune u;

	if (kbds_issearchmode() && !forcequit) {
		switch (ksym) {
			case XK_Escape:
				kbds_searchlen = 0;
				/* FALLTHROUGH */
			case XK_Return:
				for (kbds_searchcase = 0, i = 0; i < kbds_searchlen; i++) {
					if (kbds_searchstr[i].u != towlower(kbds_searchstr[i].u)) {
						kbds_searchcase = 1;
						break;
					}
				}
				count = kbds_searchall();
				kbds_searchnext(kbds_searchdir);
				kbds_selecttext();
				kbds_setmode(kbds_mode & ~KBDS_MODE_SEARCH);
				if (count == 0 && kbds_directsearch)
					ksym = XK_Escape;
				break;
			case XK_BackSpace:
				if (kbds_searchlen) {
					kbds_searchlen--;
					if (kbds_searchlen && (kbds_searchstr[kbds_searchlen].mode & ATTR_WDUMMY))
						kbds_searchlen--;
				}
				break;
			default:
				if (len < 1 || kbds_searchlen >= term.col-2)
					return 0;
				utf8decode(buf, &u, len);
				kbds_searchstr[kbds_searchlen].u = u;
				kbds_searchstr[kbds_searchlen++].mode = ATTR_NULL;
				if (wcwidth(u) > 1) {
					kbds_searchstr[kbds_searchlen-1].mode = ATTR_WIDE;
					if (kbds_searchlen < term.col-2) {
						kbds_searchstr[kbds_searchlen].u = 0;
						kbds_searchstr[kbds_searchlen++].mode = ATTR_WDUMMY;
					}
				}
				break;
		}
		/* If the direct search is aborted, we just go to the next switch
		 * statement and exit the keyboard selection mode immediately */
		if (!(ksym == XK_Escape && kbds_directsearch)) {
			term.dirty[term.row-1] = 1;
			return 0;
		}
	} else if ((kbds_mode & KBDS_MODE_FIND) && !forcequit) {
		kbds_findchar = 0;
		switch (ksym) {
			case XK_Escape:
			case XK_Return:
				kbds_quant = 0;
				break;
			default:
				if (len < 1)
					return 0;
				utf8decode(buf, &kbds_findchar, len);
				kbds_findnext(kbds_finddir, 0);
				kbds_selecttext();
				break;
		}
		kbds_setmode(kbds_mode & ~KBDS_MODE_FIND);
		return 0;
	}

	switch (ksym) {
	case -1:
		kbds_searchstr = xmalloc(term.col * sizeof(Glyph));
		kbds_in_use = 1;
		kbds_moveto(term.c.x, term.c.y);
		kbds_oc = kbds_c;
		kbds_setmode(KBDS_MODE_MOVE);
		return MODE_KBDSELECT;
	case XK_V:
		if (kbds_mode & KBDS_MODE_LSELECT) {
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		} else if (kbds_mode & KBDS_MODE_SELECT) {
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
			sel.ob.x = 0;
			tfulldirt();
			kbds_setmode((kbds_mode ^ KBDS_MODE_SELECT) | KBDS_MODE_LSELECT);
		} else {
			selstart(0, kbds_c.y, 0);
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
			kbds_setmode(kbds_mode | KBDS_MODE_LSELECT);
		}
		break;
	case XK_v:
		if (kbds_mode & KBDS_MODE_SELECT) {
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		} else if (kbds_mode & KBDS_MODE_LSELECT) {
			selextend(kbds_c.x, kbds_c.y, kbds_seltype, 0);
			kbds_setmode((kbds_mode ^ KBDS_MODE_LSELECT) | KBDS_MODE_SELECT);
		} else {
			selstart(kbds_c.x, kbds_c.y, 0);
			kbds_setmode(kbds_mode | KBDS_MODE_SELECT);
		}
		break;
	case XK_s:
		if (!(kbds_mode & KBDS_MODE_LSELECT)) {
			kbds_seltype ^= (SEL_REGULAR | SEL_RECTANGULAR);
			selextend(kbds_c.x, kbds_c.y, kbds_seltype, 0);
		}
		break;
	case XK_y:
	case XK_Y:
		if (kbds_isselectmode()) {
			kbds_copytoclipboard();
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		}
		break;
	case -2:
	case -3:
	case XK_slash:
	case XK_KP_Divide:
	case XK_question:
		kbds_directsearch = (ksym == -2 || ksym == -3);
		kbds_searchdir = (ksym == XK_question || ksym == -3) ? -1 : 1;
		kbds_searchlen = 0;
		kbds_setmode(kbds_mode | KBDS_MODE_SEARCH);
		kbds_clearhighlights();
		return 0;
	case XK_q:
	case XK_Escape:
		if (!kbds_in_use)
			return 0;
		if (kbds_quant && !forcequit) {
			kbds_quant = 0;
			break;
		}
		selclear();
		if (kbds_isselectmode() && !forcequit) {
			kbds_setmode(KBDS_MODE_MOVE);
			break;
		}
		kbds_setmode(KBDS_MODE_MOVE);
		/* FALLTHROUGH */
	case XK_Return:
		if (kbds_isselectmode())
			kbds_copytoclipboard();
		kbds_in_use = kbds_quant = 0;
		free(kbds_searchstr);
		kscrolldown(&((Arg){ .i = term.histf }));
		kbds_clearhighlights();
		return MODE_KBDSELECT;
	case XK_n:
	case XK_N:
		kbds_searchnext(ksym == XK_n ? kbds_searchdir : -kbds_searchdir);
		break;
	case XK_BackSpace:
		kbds_moveto(0, kbds_c.y);
		break;
	case XK_exclam:
		kbds_moveto(term.col/2, kbds_c.y);
		break;
	case XK_underscore:
		kbds_moveto(term.col-1, kbds_c.y);
		break;
	case XK_dollar:
	case XK_A:
		eol = kbds_c.len-1;
		line = kbds_c.line;
		islast = (kbds_c.x == eol || (kbds_c.x == eol-1 && (line[eol-1].mode & ATTR_WIDE)));
		if (islast && kbds_iswrapped(&kbds_c) && kbds_c.y < kbds_bot())
			kbds_moveto(tlinelen(TLINE(kbds_c.y+1))-1, kbds_c.y+1);
		else
			kbds_moveto(islast ? term.col-1 : eol, kbds_c.y);
		break;
	case XK_asciicircum:
	case XK_I:
		for (i = 0; i < kbds_c.len && kbds_c.line[i].u == ' '; i++)
			;
		kbds_moveto((i < kbds_c.len) ? i : 0, kbds_c.y);
		break;
	case XK_End:
	case XK_KP_End:
		kbds_moveto(kbds_c.x, term.row-1);
		break;
	case XK_Home:
	case XK_KP_Home:
	case XK_H:
		kbds_moveto(kbds_c.x, 0);
		break;
	case XK_M:
		kbds_moveto(kbds_c.x, alt ? (term.row-1) / 2
                                  : MIN(term.c.y + term.scr, term.row-1) / 2);
		break;
	case XK_L:
		kbds_moveto(kbds_c.x, alt ? term.row-1
		                          : MIN(term.c.y + term.scr, term.row-1));
		break;
	case XK_Page_Up:
	case XK_KP_Page_Up:
	case XK_K:
		prevscr = term.scr;
		kscrollup(&((Arg){ .i = term.row }));
		kbds_moveto(kbds_c.x, alt ? 0
		                          : MAX(0, kbds_c.y - term.row + term.scr - prevscr));
		break;
	case XK_Page_Down:
	case XK_KP_Page_Down:
	case XK_J:
		prevscr = term.scr;
		kscrolldown(&((Arg){ .i = term.row }));
		kbds_moveto(kbds_c.x, alt ? term.row-1
		                          : MIN(MIN(term.c.y + term.scr, term.row-1),
		                                    kbds_c.y + term.row + term.scr - prevscr));
		break;
	case XK_asterisk:
	case XK_KP_Multiply:
		kbds_moveto(term.col/2, (term.row-1) / 2);
		break;
	case XK_g:
		kscrollup(&((Arg){ .i = term.histf }));
		kbds_moveto(kbds_c.x, 0);
		break;
	case XK_G:
		kscrolldown(&((Arg){ .i = term.histf }));
		kbds_moveto(kbds_c.x, alt ? term.row-1 : term.c.y);
		break;
	case XK_b:
	case XK_B:
		kbds_nextword(1, -1, (ksym == XK_b) ? kbds_sdelim : kbds_ldelim);
		break;
	case XK_w:
	case XK_W:
		kbds_nextword(1, +1, (ksym == XK_w) ? kbds_sdelim : kbds_ldelim);
		break;
	case XK_e:
	case XK_E:
		kbds_nextword(0, +1, (ksym == XK_e) ? kbds_sdelim : kbds_ldelim);
		break;
	case XK_z:
		prevscr = term.scr;
		dy = kbds_c.y - (term.row-1) / 2;
		if (dy <= 0)
			kscrollup(&((Arg){ .i = -dy }));
		else
			kscrolldown(&((Arg){ .i = dy }));
		kbds_moveto(kbds_c.x, kbds_c.y + term.scr - prevscr);
		break;
	case XK_f:
	case XK_F:
	case XK_t:
	case XK_T:
		kbds_finddir = (ksym == XK_f || ksym == XK_t) ? 1 : -1;
		kbds_findtill = (ksym == XK_t || ksym == XK_T) ? 1 : 0;
		kbds_setmode(kbds_mode | KBDS_MODE_FIND);
		return 0;
	case XK_semicolon:
	case XK_r:
		kbds_findnext(kbds_finddir, 1);
		break;
	case XK_comma:
	case XK_R:
		kbds_findnext(-kbds_finddir, 1);
		break;
	case XK_0:
	case XK_KP_0:
		if (!kbds_quant) {
			kbds_moveto(0, kbds_c.y);
			break;
		}
		/* FALLTHROUGH */
	default:
		if (ksym >= XK_0 && ksym <= XK_9) {                 /* 0-9 keyboard */
			q = (kbds_quant * 10) + (ksym ^ XK_0);
			kbds_quant = q <= 99999999 ? q : kbds_quant;
			term.dirty[0] = 1;
			return 0;
		} else if (ksym >= XK_KP_0 && ksym <= XK_KP_9) {    /* 0-9 numpad */
			q = (kbds_quant * 10) + (ksym ^ XK_KP_0);
			kbds_quant = q <= 99999999 ? q : kbds_quant;
			term.dirty[0] = 1;
			return 0;
		} else if (ksym == XK_k || ksym == XK_h)
			i = ksym & 1;
		else if (ksym == XK_l || ksym == XK_j)
			i = ((ksym & 6) | 4) >> 1;
		else if (ksym >= XK_KP_Left && ksym <= XK_KP_Down)
			i = ksym - XK_KP_Left;
		else if ((XK_Home & ksym) != XK_Home || (i = (ksym ^ XK_Home) - 1) > 3)
			return 0;

		kbds_quant = (kbds_quant ? kbds_quant : 1);

		if (i & 1) {
			kbds_c.y += kbds_quant * (i & 2 ? 1 : -1);
		} else {
			for (;kbds_quant > 0; kbds_quant--) {
				if (!kbds_moveforward(&kbds_c, (i & 2) ? 1 : -1,
					    KBDS_WRAP_LINE | KBDS_WRAP_EDGE))
					break;
			}
		}
		kbds_moveto(kbds_c.x, kbds_c.y);
	}
	kbds_selecttext();
	kbds_quant = 0;
	term.dirty[0] = 1;
	return 0;
}
