void
tloaddefscreen(int clear, int loadcursor)
{
	int col, row, alt = IS_SET(MODE_ALTSCREEN);

	if (alt) {
		if (clear) {
			tclearregion(0, 0, term.col-1, term.row-1, 1);
			#if SIXEL_PATCH
			tdeleteimages();
			#endif // SIXEL_PATCH
		}
		col = term.col, row = term.row;
		tswapscreen();
	}
	if (loadcursor)
		tcursor(CURSOR_LOAD);
	if (alt)
		tresizedef(col, row);
}

void
tloadaltscreen(int clear, int savecursor)
{
	int col, row, def = !IS_SET(MODE_ALTSCREEN);

	if (savecursor)
		tcursor(CURSOR_SAVE);
	if (def) {
		col = term.col, row = term.row;
		kscrolldown(&((Arg){ .i = term.scr }));
		tswapscreen();
		tresizealt(col, row);
	}
	if (clear) {
		tclearregion(0, 0, term.col-1, term.row-1, 1);
		#if SIXEL_PATCH
		tdeleteimages();
		#endif // SIXEL_PATCH
	}
}

void
selmove(int n)
{
	sel.ob.y += n, sel.nb.y += n;
	sel.oe.y += n, sel.ne.y += n;
}

void
tclearglyph(Glyph *gp, int usecurattr)
{
	if (usecurattr) {
		gp->fg = term.c.attr.fg;
		gp->bg = term.c.attr.bg;
	} else {
		gp->fg = defaultfg;
		gp->bg = defaultbg;
	}
	gp->mode = ATTR_NULL;
	gp->u = ' ';
}

#if SIXEL_PATCH
void
treflow_moveimages(int oldy, int newy)
{
	ImageList *im;

	for (im = term.images; im; im = im->next) {
		if (im->y == oldy)
			im->reflow_y = newy;
	}
}
#endif // SIXEL_PATCH

void
treflow(int col, int row)
{
	int i, j;
	int oce, nce, bot, scr;
	int ox = 0, oy = -term.histf, nx = 0, ny = -1, len;
	int cy = -1; /* proxy for new y coordinate of cursor */
	int buflen, nlines;
	Line *buf, bufline, line;
	#if SIXEL_PATCH
	ImageList *im, *next;

	for (im = term.images; im; im = im->next)
		im->reflow_y = INT_MIN; /* unset reflow_y */
	#endif // SIXEL_PATCH

	/* y coordinate of cursor line end */
	for (oce = term.c.y; oce < term.row - 1 &&
	                     tiswrapped(term.line[oce]); oce++);

	nlines = HISTSIZE + row;
	buf = xmalloc(nlines * sizeof(Line));
	do {
		if (!nx && ++ny < nlines)
			buf[ny] = xmalloc(col * sizeof(Glyph));
		if (!ox) {
			line = TLINEABS(oy);
			len = tlinelen(line);
		}
		if (oy == term.c.y) {
			if (!ox)
				len = MAX(len, term.c.x + 1);
			/* update cursor */
			if (cy < 0 && term.c.x - ox < col - nx) {
				term.c.x = nx + term.c.x - ox, cy = ny;
				UPDATEWRAPNEXT(0, col);
			}
		}
		/* get reflowed lines in buf */
		bufline = buf[ny % nlines];
		if (col - nx > len - ox) {
			memcpy(&bufline[nx], &line[ox], (len-ox) * sizeof(Glyph));
			nx += len - ox;
			if (len == 0 || !(line[len - 1].mode & ATTR_WRAP)) {
				for (j = nx; j < col; j++)
					tclearglyph(&bufline[j], 0);
				#if SIXEL_PATCH
				treflow_moveimages(oy+term.scr, ny);
				#endif // SIXEL_PATCH
				nx = 0;
			} else if (nx > 0) {
				bufline[nx - 1].mode &= ~ATTR_WRAP;
			}
			ox = 0, oy++;
		} else if (col - nx == len - ox) {
			memcpy(&bufline[nx], &line[ox], (col-nx) * sizeof(Glyph));
			#if SIXEL_PATCH
			treflow_moveimages(oy+term.scr, ny);
			#endif // SIXEL_PATCH
			ox = 0, oy++, nx = 0;
		} else/* if (col - nx < len - ox) */ {
			memcpy(&bufline[nx], &line[ox], (col-nx) * sizeof(Glyph));
			if (bufline[col - 1].mode & ATTR_WIDE) {
				bufline[col - 2].mode |= ATTR_WRAP;
				tclearglyph(&bufline[col - 1], 0);
				ox--;
			} else {
				bufline[col - 1].mode |= ATTR_WRAP;
			}
			#if SIXEL_PATCH
			treflow_moveimages(oy+term.scr, ny);
			#endif // SIXEL_PATCH
			ox += col - nx;
			nx = 0;
		}
	} while (oy <= oce);
	if (nx)
		for (j = nx; j < col; j++)
			tclearglyph(&bufline[j], 0);

	/* free extra lines */
	for (i = row; i < term.row; i++)
		free(term.line[i]);
	/* resize to new height */
	term.line = xrealloc(term.line, row * sizeof(Line));

	buflen = MIN(ny + 1, nlines);
	bot = MIN(ny, row - 1);
	scr = MAX(row - term.row, 0);
	/* update y coordinate of cursor line end */
	nce = MIN(oce + scr, bot);
	/* update cursor y coordinate */
	term.c.y = nce - (ny - cy);
	if (term.c.y < 0) {
		j = nce, nce = MIN(nce + -term.c.y, bot);
		term.c.y += nce - j;
		while (term.c.y < 0) {
			free(buf[ny-- % nlines]);
			buflen--;
			term.c.y++;
		}
	}
	/* allocate new rows */
	for (i = row - 1; i > nce; i--) {
		if (i >= term.row)
			term.line[i] = xmalloc(col * sizeof(Glyph));
		else
			term.line[i] = xrealloc(term.line[i], col * sizeof(Glyph));
		for (j = 0; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* fill visible area */
	for (/*i = nce */; i >= term.row; i--, ny--, buflen--)
		term.line[i] = buf[ny % nlines];
	for (/*i = term.row - 1 */; i >= 0; i--, ny--, buflen--) {
		free(term.line[i]);
		term.line[i] = buf[ny % nlines];
	}
	/* fill lines in history buffer and update term.histf */
	for (/*i = -1 */; buflen > 0 && i >= -HISTSIZE; i--, ny--, buflen--) {
		j = (term.histi + i + 1 + HISTSIZE) % HISTSIZE;
		free(term.hist[j]);
		term.hist[j] = buf[ny % nlines];
	}
	term.histf = -i - 1;
	term.scr = MIN(term.scr, term.histf);
	/* resize rest of the history lines */
	for (/*i = -term.histf - 1 */; i >= -HISTSIZE; i--) {
		j = (term.histi + i + 1 + HISTSIZE) % HISTSIZE;
		term.hist[j] = xrealloc(term.hist[j], col * sizeof(Glyph));
	}

	#if SIXEL_PATCH
	/* move images to the final position */
	for (im = term.images; im; im = next) {
		next = im->next;
		if (im->reflow_y == INT_MIN) {
			delete_image(im);
		} else {
			im->y = im->reflow_y - term.histf + term.scr - (ny + 1);
			if (im->y - term.scr < -HISTSIZE || im->y - term.scr >= row)
				delete_image(im);
		}
	}

	/* expand images into new text cells */
	for (im = term.images; im; im = im->next) {
		j = MIN(im->x + im->cols, col);
		line = TLINE(im->y);
		for (i = im->x; i < j; i++) {
			if (!(line[i].mode & ATTR_SET))
				line[i].mode |= ATTR_SIXEL;
		}
	}
	#endif // SIXEL_PATCH

	for (; buflen > 0; ny--, buflen--)
		free(buf[ny % nlines]);
	free(buf);
}

void
rscrolldown(int n)
{
	int i;
	Line temp;

	/* can never be true as of now
	if (IS_SET(MODE_ALTSCREEN))
		return; */

	if ((n = MIN(n, term.histf)) <= 0)
		return;

	for (i = term.c.y + n; i >= n; i--) {
		temp = term.line[i];
		term.line[i] = term.line[i-n];
		term.line[i-n] = temp;
	}
	for (/*i = n - 1 */; i >= 0; i--) {
		temp = term.line[i];
		term.line[i] = term.hist[term.histi];
		term.hist[term.histi] = temp;
		term.histi = (term.histi - 1 + HISTSIZE) % HISTSIZE;
	}
	term.c.y += n;
	term.histf -= n;
	if ((i = term.scr - n) >= 0) {
		term.scr = i;
	} else {
		#if SIXEL_PATCH
		scroll_images(n - term.scr);
		#endif // SIXEL_PATCH
		term.scr = 0;
		if (sel.ob.x != -1 && !sel.alt)
			selmove(-i);
	}
}

void
tresizedef(int col, int row)
{
	int i, j;

	/* return if dimensions haven't changed */
	if (term.col == col && term.row == row) {
		tfulldirt();
		return;
	}
	if (col != term.col) {
		if (!sel.alt)
			selremove();
		treflow(col, row);
	} else {
		/* slide screen up if otherwise cursor would get out of the screen */
		if (term.c.y >= row) {
			tscrollup(0, term.row - 1, term.c.y - row + 1, SCROLL_RESIZE);
			term.c.y = row - 1;
		}
		for (i = row; i < term.row; i++)
			free(term.line[i]);

		/* resize to new height */
		term.line = xrealloc(term.line, row * sizeof(Line));
		/* allocate any new rows */
		for (i = term.row; i < row; i++) {
			term.line[i] = xmalloc(col * sizeof(Glyph));
			for (j = 0; j < col; j++)
				tclearglyph(&term.line[i][j], 0);
		}
		/* scroll down as much as height has increased */
		rscrolldown(row - term.row);
	}
	/* update terminal size */
	term.col = col, term.row = row;
	/* reset scrolling region */
	term.top = 0, term.bot = row - 1;
	/* dirty all lines */
	tfulldirt();
}

void
tresizealt(int col, int row)
{
	int i, j;
	#if SIXEL_PATCH
	ImageList *im, *next;
	#endif // SIXEL_PATCH

	/* return if dimensions haven't changed */
	if (term.col == col && term.row == row) {
		tfulldirt();
		return;
	}
	if (sel.alt)
		selremove();
	/* slide screen up if otherwise cursor would get out of the screen */
	for (i = 0; i <= term.c.y - row; i++)
		free(term.line[i]);
	if (i > 0) {
		/* ensure that both src and dst are not NULL */
		memmove(term.line, term.line + i, row * sizeof(Line));
		#if SIXEL_PATCH
		scroll_images(-i);
		#endif // SIXEL_PATCH
		term.c.y = row - 1;
	}
	for (i += row; i < term.row; i++)
		free(term.line[i]);
	/* resize to new height */
	term.line = xrealloc(term.line, row * sizeof(Line));
	/* resize to new width */
	for (i = 0; i < MIN(row, term.row); i++) {
		term.line[i] = xrealloc(term.line[i], col * sizeof(Glyph));
		for (j = term.col; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* allocate any new rows */
	for (/*i = MIN(row, term.row) */; i < row; i++) {
		term.line[i] = xmalloc(col * sizeof(Glyph));
		for (j = 0; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* update cursor */
	if (term.c.x >= col) {
		term.c.state &= ~CURSOR_WRAPNEXT;
		term.c.x = col - 1;
	} else {
		UPDATEWRAPNEXT(1, col);
	}
	/* update terminal size */
	term.col = col, term.row = row;
	/* reset scrolling region */
	term.top = 0, term.bot = row - 1;

	#if SIXEL_PATCH
	/* delete or clip images if they are not inside the screen */
	for (im = term.images; im; im = next) {
		next = im->next;
		if (im->x >= term.col || im->y >= term.row || im->y < 0) {
			delete_image(im);
		} else {
			if ((im->cols = MIN(im->x + im->cols, term.col) - im->x) <= 0)
				delete_image(im);
		}
	}
	#endif // SIXEL_PATCH

	/* dirty all lines */
	tfulldirt();
}

void
kscrolldown(const Arg* a)
{
	int n = a->i;

	if (!term.scr || IS_SET(MODE_ALTSCREEN))
		return;

	if (n < 0)
		n = MAX(term.row / -n, 1);

	if (n <= term.scr) {
		term.scr -= n;
	} else {
		n = term.scr;
		term.scr = 0;
	}

	if (sel.ob.x != -1 && !sel.alt)
		selmove(-n); /* negate change in term.scr */
	tfulldirt();

	#if SIXEL_PATCH
	scroll_images(-1*n);
	#endif // SIXEL_PATCH

	#if OPENURLONCLICK_PATCH
	if (n > 0)
		restoremousecursor();
	#endif // OPENURLONCLICK_PATCH
}

void
kscrollup(const Arg* a)
{
	int n = a->i;

	if (!term.histf || IS_SET(MODE_ALTSCREEN))
		return;

	if (n < 0)
		n = MAX(term.row / -n, 1);

	if (term.scr + n <= term.histf) {
		term.scr += n;
	} else {
		n = term.histf - term.scr;
		term.scr = term.histf;
	}

	if (sel.ob.x != -1 && !sel.alt)
		selmove(n); /* negate change in term.scr */
	tfulldirt();

	#if SIXEL_PATCH
	scroll_images(n);
	#endif // SIXEL_PATCH

	#if OPENURLONCLICK_PATCH
	if (n > 0)
		restoremousecursor();
	#endif // OPENURLONCLICK_PATCH
}

void
tscrollup(int top, int bot, int n, int mode)
{
	#if OPENURLONCLICK_PATCH
	restoremousecursor();
	#endif //OPENURLONCLICK_PATCH

	int i, j, s;
	Line temp;
	int alt = IS_SET(MODE_ALTSCREEN);
	int savehist = !alt && top == 0 && mode != SCROLL_NOSAVEHIST;
	int scr = alt ? 0 : term.scr;
	#if SIXEL_PATCH
	int itop = top + scr, ibot = bot + scr;
	ImageList *im, *next;
	#endif // SIXEL_PATCH

	if (n <= 0)
		return;
	n = MIN(n, bot-top+1);

	if (savehist) {
		for (i = 0; i < n; i++) {
			term.histi = (term.histi + 1) % HISTSIZE;
			temp = term.hist[term.histi];
			for (j = 0; j < term.col; j++)
				tclearglyph(&temp[j], 1);
			term.hist[term.histi] = term.line[i];
			term.line[i] = temp;
		}
		term.histf = MIN(term.histf + n, HISTSIZE);
		s = n;
		if (term.scr) {
			j = term.scr;
			term.scr = MIN(j + n, HISTSIZE);
			s = j + n - term.scr;
		}
		if (mode != SCROLL_RESIZE)
			tfulldirt();
	} else {
		tclearregion(0, top, term.col-1, top+n-1, 1);
		tsetdirt(top + scr, bot + scr);
	}

	for (i = top; i <= bot-n; i++) {
		temp = term.line[i];
		term.line[i] = term.line[i+n];
		term.line[i+n] = temp;
	}

	#if SIXEL_PATCH
	if (alt || !savehist) {
		/* move images, if they are inside the scrolling region */
		for (im = term.images; im; im = next) {
			next = im->next;
			if (im->y >= itop && im->y <= ibot) {
				im->y -= n;
				if (im->y < itop)
					delete_image(im);
			}
		}
	} else {
		/* move images, if they are inside the scrolling region or scrollback */
		for (im = term.images; im; im = next) {
			next = im->next;
			im->y -= scr;
			if (im->y < 0) {
				im->y -= n;
			} else if (im->y >= top && im->y <= bot) {
				im->y -= n;
				if (im->y < top)
					im->y -= top; // move to scrollback
			}
			if (im->y < -HISTSIZE)
				delete_image(im);
			else
				im->y += term.scr;
		}
	}
	#endif // SIXEL_PATCH

	if (sel.ob.x != -1 && sel.alt == alt) {
		if (!savehist) {
			selscroll(top, bot, -n);
		} else if (s > 0) {
			selmove(-s);
			if (-term.scr + sel.nb.y < -term.histf)
				selremove();
		}
	}
}

void
tscrolldown(int top, int n)
{
	#if OPENURLONCLICK_PATCH
	restoremousecursor();
	#endif //OPENURLONCLICK_PATCH

	int i, bot = term.bot;
	int scr = IS_SET(MODE_ALTSCREEN) ? 0 : term.scr;
	int itop = top + scr, ibot = bot + scr;
	Line temp;
	#if SIXEL_PATCH
	ImageList *im, *next;
	#endif // SIXEL_PATCH

	if (n <= 0)
		return;
	n = MIN(n, bot-top+1);

	tsetdirt(top + scr, bot + scr);
	tclearregion(0, bot-n+1, term.col-1, bot, 1);

	for (i = bot; i >= top+n; i--) {
		temp = term.line[i];
		term.line[i] = term.line[i-n];
		term.line[i-n] = temp;
	}

	#if SIXEL_PATCH
	/* move images, if they are inside the scrolling region */
	for (im = term.images; im; im = next) {
		next = im->next;
		if (im->y >= itop && im->y <= ibot) {
			im->y += n;
			if (im->y > ibot)
				delete_image(im);
		}
	}
	#endif // SIXEL_PATCH

	if (sel.ob.x != -1 && sel.alt == IS_SET(MODE_ALTSCREEN))
		selscroll(top, bot, n);
}

void
tresize(int col, int row)
{
	int *bp;

	#if KEYBOARDSELECT_PATCH
	if (row != term.row || col != term.col)
		win.mode ^= kbds_keyboardhandler(XK_Escape, NULL, 0, 1);
	#endif // KEYBOARDSELECT_PATCH

	term.dirty = xrealloc(term.dirty, row * sizeof(*term.dirty));
	term.tabs = xrealloc(term.tabs, col * sizeof(*term.tabs));
	if (col > term.col) {
		bp = term.tabs + term.col;
		memset(bp, 0, sizeof(*term.tabs) * (col - term.col));
		while (--bp > term.tabs && !*bp)
			/* nothing */ ;
		for (bp += tabspaces; bp < term.tabs + col; bp += tabspaces)
			*bp = 1;
	}

	if (IS_SET(MODE_ALTSCREEN))
		tresizealt(col, row);
	else
		tresizedef(col, row);
}

void
tclearregion(int x1, int y1, int x2, int y2, int usecurattr)
{
	int x, y;

	/* regionselected() takes relative coordinates */
	if (regionselected(x1+term.scr, y1+term.scr, x2+term.scr, y2+term.scr))
		selremove();

	for (y = y1; y <= y2; y++) {
		term.dirty[y] = 1;
		for (x = x1; x <= x2; x++)
			tclearglyph(&term.line[y][x], usecurattr);
	}
}

void
tnew(int col, int row)
{
	int i, j;
	for (i = 0; i < 2; i++) {
		term.line = xmalloc(row * sizeof(Line));
		for (j = 0; j < row; j++)
			term.line[j] = xmalloc(col * sizeof(Glyph));
		term.col = col, term.row = row;
		tswapscreen();
	}
	term.dirty = xmalloc(row * sizeof(*term.dirty));
	term.tabs = xmalloc(col * sizeof(*term.tabs));
	for (i = 0; i < HISTSIZE; i++)
		term.hist[i] = xmalloc(col * sizeof(Glyph));
	treset();
}

void
tdeletechar(int n)
{
	int src, dst, size;
	Line line;

	if (n <= 0)
		return;
	dst = term.c.x;
	src = MIN(term.c.x + n, term.col);
	size = term.col - src;
	if (size > 0) { /* otherwise src would point beyond the array
	                   https://stackoverflow.com/questions/29844298 */
		line = term.line[term.c.y];
		memmove(&line[dst], &line[src], size * sizeof(Glyph));
	}
	tclearregion(dst + size, term.c.y, term.col - 1, term.c.y, 1);
}

void
tinsertblank(int n)
{
	int src, dst, size;
	Line line;

	if (n <= 0)
		return;
	dst = MIN(term.c.x + n, term.col);
	src = term.c.x;
	size = term.col - dst;

	if (size > 0) { /* otherwise dst would point beyond the array */
		line = term.line[term.c.y];
		memmove(&line[dst], &line[src], size * sizeof(Glyph));
	}
	tclearregion(src, term.c.y, dst - 1, term.c.y, 1);
}

int
tlinelen(Line line)
{
	int i = term.col - 1;

	/* We are using a different algorithm on the alt screen because an
	 * application might use spaces to clear the screen and in that case it is
	 * impossible to find the end of the line when every cell has the ATTR_SET
	 * attribute. The second algorithm is more accurate on the main screen and
	 * and we can use it there. */
	if (IS_SET(MODE_ALTSCREEN))
		for (; i >= 0 && !(line[i].mode & ATTR_WRAP) && line[i].u == ' '; i--);
	else
		for (; i >= 0 && !(line[i].mode & (ATTR_SET | ATTR_WRAP)); i--);

	return i + 1;
}

int
tiswrapped(Line line)
{
	int len = tlinelen(line);

	return len > 0 && (line[len - 1].mode & ATTR_WRAP);
}

char *
tgetglyphs(char *buf, const Glyph *gp, const Glyph *lgp)
{
	while (gp <= lgp)
		if (gp->mode & ATTR_WDUMMY) {
			gp++;
		} else {
			buf += utf8encode((gp++)->u, buf);
		}
	return buf;
}

size_t
tgetline(char *buf, const Glyph *fgp)
{
	char *ptr;
	const Glyph *lgp = &fgp[term.col - 1];

	while (lgp > fgp && !(lgp->mode & (ATTR_SET | ATTR_WRAP)))
		lgp--;
	ptr = tgetglyphs(buf, fgp, lgp);
	if (!(lgp->mode & ATTR_WRAP))
		*(ptr++) = '\n';
	return ptr - buf;
}

int
regionselected(int x1, int y1, int x2, int y2)
{
	if (sel.ob.x == -1 || sel.mode == SEL_EMPTY ||
	    sel.alt != IS_SET(MODE_ALTSCREEN) || sel.nb.y > y2 || sel.ne.y < y1)
		return 0;

	return (sel.type == SEL_RECTANGULAR) ? sel.nb.x <= x2 && sel.ne.x >= x1
		: (sel.nb.y != y2 || sel.nb.x <= x2) &&
		  (sel.ne.y != y1 || sel.ne.x >= x1);
}

int
selected(int x, int y)
{
	return regionselected(x, y, x, y);
}

void
selsnap(int *x, int *y, int direction)
{
	int newx, newy;
	int rtop = 0, rbot = term.row - 1;
	int delim, prevdelim, maxlen;
	const Glyph *gp, *prevgp;

	if (!IS_SET(MODE_ALTSCREEN))
		rtop += -term.histf + term.scr, rbot += term.scr;

	switch (sel.snap) {
	case SNAP_WORD:
		/*
		 * Snap around if the word wraps around at the end or
		 * beginning of a line.
		 */
		maxlen = (TLINE(*y)[term.col-2].mode & ATTR_WRAP) ? term.col-1 : term.col;
		LIMIT(*x, 0, maxlen - 1);
		prevgp = &TLINE(*y)[*x];
		prevdelim = ISDELIM(prevgp->u);
		for (;;) {
			newx = *x + direction;
			newy = *y;
			if (!BETWEEN(newx, 0, maxlen - 1)) {
				newy += direction;
				if (!BETWEEN(newy, rtop, rbot))
					break;

				if (!tiswrapped(TLINE(direction > 0 ? *y : newy)))
					break;

				maxlen = (TLINE(newy)[term.col-2].mode & ATTR_WRAP) ? term.col-1 : term.col;
				newx = direction > 0 ? 0 : maxlen - 1;
			}

			gp = &TLINE(newy)[newx];
			delim = ISDELIM(gp->u);
			if (!(gp->mode & ATTR_WDUMMY) && (delim != prevdelim
					|| (delim && gp->u != prevgp->u)))
				break;

			*x = newx;
			*y = newy;
			if (!(gp->mode & ATTR_WDUMMY)) {
				prevgp = gp;
				prevdelim = delim;
			}
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
			for (; *y > rtop; *y -= 1) {
				if (!tiswrapped(TLINE(*y-1)))
					break;
			}
		} else if (direction > 0) {
			for (; *y < rbot; *y += 1) {
				if (!tiswrapped(TLINE(*y)))
					break;
			}
		}
		break;
	}
}

void
selscroll(int top, int bot, int n)
{
	/* turn absolute coordinates into relative */
	top += term.scr, bot += term.scr;

	if (BETWEEN(sel.nb.y, top, bot) != BETWEEN(sel.ne.y, top, bot)) {
		selclear();
	} else if (BETWEEN(sel.nb.y, top, bot)) {
		selmove(n);
		if (sel.nb.y < top || sel.ne.y > bot)
			selclear();
	}
}

void
tswapscreen(void)
{
	static Line *altline;
	static int altcol, altrow;
	Line *tmpline = term.line;
	int tmpcol = term.col, tmprow = term.row;
	#if SIXEL_PATCH
	ImageList *im = term.images;
	#endif // SIXEL_PATCH

	term.line = altline;
	term.col = altcol, term.row = altrow;
	altline = tmpline;
	altcol = tmpcol, altrow = tmprow;
	term.mode ^= MODE_ALTSCREEN;

	#if SIXEL_PATCH
	term.images = term.images_alt;
	term.images_alt = im;
	#endif // SIXEL_PATCH
}

char *
getsel(void)
{
	char *str, *ptr;
	int y, lastx, linelen;
	const Glyph *gp, *lgp;

	if (sel.ob.x == -1 || sel.alt != IS_SET(MODE_ALTSCREEN))
		return NULL;

	str = xmalloc((term.col + 1) * (sel.ne.y - sel.nb.y + 1) * UTF_SIZ);
	ptr = str;

	/* append every set & selected glyph to the selection */
	for (y = sel.nb.y; y <= sel.ne.y; y++) {
		Line line = TLINE(y);

		if ((linelen = tlinelen(line)) == 0) {
			*ptr++ = '\n';
			continue;
		}

		if (sel.type == SEL_RECTANGULAR) {
			gp = &line[sel.nb.x];
			lastx = sel.ne.x;
		} else {
			gp = &line[sel.nb.y == y ? sel.nb.x : 0];
			lastx = (sel.ne.y == y) ? sel.ne.x : term.col-1;
		}
		lgp = &line[MIN(lastx, linelen-1)];

		ptr = tgetglyphs(ptr, gp, lgp);
		/*
		 * Copy and pasting of line endings is inconsistent
		 * in the inconsistent terminal and GUI world.
		 * The best solution seems like to produce '\n' when
		 * something is copied from st and convert '\n' to
		 * '\r', when something to be pasted is received by
		 * st.
		 * FIXME: Fix the computer world.
		 */
		if ((y < sel.ne.y || lastx >= linelen) &&
		    (!(lgp->mode & ATTR_WRAP) || sel.type == SEL_RECTANGULAR))
			*ptr++ = '\n';
	}
	*ptr = '\0';
	return str;
}

void
tdumpline(int n)
{
	char str[(term.col + 1) * UTF_SIZ];

	tprinter(str, tgetline(str, &term.line[n][0]));
}
