int
tiswrapped(Line line)
{
	int len = tlinelen(line);

	return len > 0 && (line[len - 1].mode & ATTR_WRAP);
}

char *
tgetline(char *buf, const Glyph *gp, const Glyph *last, int gettab)
{
	while (gp <= last)
		if (gp->mode & ATTR_WDUMMY) {
			gp++;
		} else if (gettab && gp->state == GLYPH_TAB) {
			*(buf++) = '\t';
			while (++gp <= last && gp->state == GLYPH_TDUMMY);
		} else {
			buf += utf8encode((gp++)->u, buf);
		}
	return buf;
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

void
selremove(void)
{
	sel.mode = SEL_IDLE;
	sel.ob.x = -1;
}

void
tresetcursor(void)
{
	term.c = (TCursor){ { .mode = ATTR_NULL, .fg = defaultfg, .bg = defaultbg },
	                    .x = 0, .y = 0, .state = CURSOR_DEFAULT };
}

void
tloaddefscreen(int clear, int loadcursor)
{
	int col, row, alt = IS_SET(MODE_ALTSCREEN);

	if (alt) {
		if (clear)
			tclearregion(0, 0, term.col-1, term.row-1, 1);
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
		tswapscreen();
		term.scr = 0;
		tresizealt(col, row);
	}
	if (clear)
		tclearregion(0, 0, term.col-1, term.row-1, 1);
}

int
tisaltscreen(void)
{
	return IS_SET(MODE_ALTSCREEN);
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
	gp->state = GLYPH_EMPTY;
	gp->u = ' ';
}

void
twritetab(void)
{
	int x = term.c.x, y = term.c.y;

	/* selected() takes relative coordinates */
	if (selected(x + term.scr, y + term.scr))
		selclear();

	term.line[y][x].u = ' ';
	term.line[y][x].state = GLYPH_TAB;

	while (++x < term.col && !term.tabs[x]) {
		term.line[y][x].u = ' ';
		term.line[y][x].state = GLYPH_TDUMMY;
	}

	term.c.x = MIN(x, term.col-1);
}

void
treflow(int col, int row)
{
	int i, j;
	int oce, nce, bot, scr;
	int ox = 0, oy = -term.histf, nx = 0, ny = -1, len;
	int cy = -1; /* proxy for new y coordinate of cursor */
	int nlines;
	Line *buf, line;

	/* y coordinate of cursor line end */
	for (oce = term.c.y; oce < term.row - 1 &&
	                     tiswrapped(term.line[oce]); oce++);

	nlines = term.histf + oce + 1;
	if (col < term.col) {
		/* each line can take this many lines after reflow */
		j = (term.col + col - 1) / col;
		nlines = j * nlines;
		if (nlines > HISTSIZE + RESIZEBUFFER + row) {
			nlines = HISTSIZE + RESIZEBUFFER + row;
			oy = -(nlines / j - oce - 1);
		}
	}
	buf = xmalloc(nlines * sizeof(Line));
	do {
		if (!nx)
			buf[++ny] = xmalloc(col * sizeof(Glyph));
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
		if (col - nx > len - ox) {
			memcpy(&buf[ny][nx], &line[ox], (len-ox) * sizeof(Glyph));
			nx += len - ox;
			if (len == 0 || !(line[len - 1].mode & ATTR_WRAP)) {
				for (j = nx; j < col; j++)
					tclearglyph(&buf[ny][j], 0);
				nx = 0;
			} else {
				buf[ny][nx - 1].mode &= ~ATTR_WRAP;
			}
			ox = 0, oy++;
		} else if (col - nx == len - ox) {
			memcpy(&buf[ny][nx], &line[ox], (col-nx) * sizeof(Glyph));
			ox = 0, oy++, nx = 0;
		} else/* if (col - nx < len - ox) */ {
			memcpy(&buf[ny][nx], &line[ox], (col-nx) * sizeof(Glyph));
			for (ox += col - nx; ox < len &&
			                     line[ox].mode == GLYPH_TDUMMY; ox++);
			if (ox == len) {
				ox = 0, oy++;
			} else {
				buf[ny][col - 1].mode |= ATTR_WRAP;
			}
			nx = 0;
		}
	} while (oy <= oce);
	if (nx)
		for (j = nx; j < col; j++)
			tclearglyph(&buf[ny][j], 0);

	/* free extra lines */
	for (i = row; i < term.row; i++)
		free(term.line[i]);
	/* resize to new height */
	term.line = xrealloc(term.line, row * sizeof(Line));

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
			free(buf[ny--]);
			term.c.y++;
		}
	}
	/* allocate new rows */
	for (i = row - 1; i > nce; i--) {
		term.line[i] = xmalloc(col * sizeof(Glyph));
		for (j = 0; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* fill visible area */
	for (/*i = nce */; i >= term.row; i--, ny--)
		term.line[i] = buf[ny];
	for (/*i = term.row - 1 */; i >= 0; i--, ny--) {
		free(term.line[i]);
		term.line[i] = buf[ny];
	}
	/* fill lines in history buffer and update term.histf */
	for (/*i = -1 */; ny >= 0 && i >= -HISTSIZE; i--, ny--) {
		j = (term.histi + i + 1 + HISTSIZE) % HISTSIZE;
		free(term.hist[j]);
		term.hist[j] = buf[ny];
	}
	term.histf = -i - 1;
	term.scr = MIN(term.scr, term.histf);
	/* resize rest of the history lines */
	for (/*i = -term.histf - 1 */; i >= -HISTSIZE; i--) {
		j = (term.histi + i + 1 + HISTSIZE) % HISTSIZE;
		term.hist[j] = xrealloc(term.hist[j], col * sizeof(Glyph));
	}
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
	/* dirty all lines */
	tfulldirt();
}