void set_notifmode(int type, KeySym ksym)
{
	static char *lib[] = { " MOVE ", " SEL  "};
	static Glyph *g, *deb, *fin;
	static int col, bot;

	if (ksym == -1) {
		free(g);
		col = term.col, bot = term.bot;
		g = xmalloc(col * sizeof(Glyph));
		memcpy(g, term.line[bot], col * sizeof(Glyph));

	} else if (ksym == -2)
		memcpy(term.line[bot], g, col * sizeof(Glyph));

	if ( type < 2 ) {
		char *z = lib[type];
		for (deb = &term.line[bot][col - 6], fin = &term.line[bot][col]; deb < fin; z++, deb++)
			deb->mode = ATTR_REVERSE,
			deb->u = *z,
			deb->fg = defaultfg, deb->bg = defaultbg;
	} else if (type < 5)
		memcpy(term.line[bot], g, col * sizeof(Glyph));
	else {
		for (deb = &term.line[bot][0], fin = &term.line[bot][col]; deb < fin; deb++)
			deb->mode = ATTR_REVERSE,
			deb->u = ' ',
			deb->fg = defaultfg, deb->bg = defaultbg;
		term.line[bot][0].u = ksym;
	}

	term.dirty[bot] = 1;
	drawregion(0, bot, col, bot + 1);
}

#if SCROLLBACK_PATCH && KEYBOARDSELECT_PATCH
Glyph getglyph(Term term, int y, int x)
{
	Glyph g;
	int realy = y - term.scr;
	if(realy >= 0) {
		g = term.line[realy][x];
	} else {
		realy = term.histi - term.scr + y + 1;
		g = term.hist[realy][x];
	}
	return g;
}
#endif

void select_or_drawcursor(int selectsearch_mode, int type)
{
	int done = 0;

	if (selectsearch_mode & 1) {
		selextend(term.c.x, term.c.y, type, done);
		xsetsel(getsel());
	} else {
		#if LIGATURES_PATCH
		xdrawcursor(term.c.x, term.c.y, term.line[term.c.y][term.c.x],
					term.ocx, term.ocy, term.line[term.ocy][term.ocx],
					term.line[term.ocy], term.col);
		#elif SCROLLBACK_PATCH && KEYBOARDSELECT_PATCH
		xdrawcursor(term.c.x, term.c.y, getglyph(term, term.c.y, term.c.x),
		            term.ocx, term.ocy, getglyph(term, term.ocy, term.ocx));
		#else
		xdrawcursor(term.c.x, term.c.y, term.line[term.c.y][term.c.x],
					term.ocx, term.ocy, term.line[term.ocy][term.ocx]);
		#endif // LIGATURES_PATCH
	}
}

void search(int selectsearch_mode, Rune *target, int ptarget, int incr, int type, TCursor *cu)
{
	Rune *r;
	int i, bound = (term.col * cu->y + cu->x) * (incr > 0) + incr;

	for (i = term.col * term.c.y + term.c.x + incr; i != bound; i += incr) {
		for (r = target; r - target < ptarget; r++) {
			if (*r == term.line[(i + r - target) / term.col][(i + r - target) % term.col].u) {
				if (r - target == ptarget - 1)
					break;
			} else {
				r = NULL;
				break;
			}
		}
		if (r != NULL)
			break;
	}

	if (i != bound) {
		term.c.y = i / term.col, term.c.x = i % term.col;
		select_or_drawcursor(selectsearch_mode, type);
	}
}

int trt_kbdselect(KeySym ksym, char *buf, int len)
{
	static TCursor cu;
	static Rune target[64];
	static int type = 1, ptarget, in_use;
	static int sens, quant;
	static char selectsearch_mode;
	int i, bound, *xy;

	if (selectsearch_mode & 2) {
		if (ksym == XK_Return) {
			selectsearch_mode ^= 2;
			set_notifmode(selectsearch_mode, -2);
			if (ksym == XK_Escape)
				ptarget = 0;
			return 0;
		} else if (ksym == XK_BackSpace) {
			if (!ptarget)
				return 0;
			term.line[term.bot][ptarget--].u = ' ';
		} else if (len < 1) {
			return 0;
		} else if (ptarget == term.col || ksym == XK_Escape) {
			return 0;
		} else {
			utf8decode(buf, &target[ptarget++], len);
			term.line[term.bot][ptarget].u = target[ptarget - 1];
		}

		if (ksym != XK_BackSpace)
			search(selectsearch_mode, &target[0], ptarget, sens, type, &cu);

		term.dirty[term.bot] = 1;
		drawregion(0, term.bot, term.col, term.bot + 1);
		return 0;
	}

	switch (ksym) {
	case -1:
		in_use = 1;
		cu.x = term.c.x, cu.y = term.c.y;
		set_notifmode(0, ksym);
		return MODE_KBDSELECT;
	case XK_s:
		if (selectsearch_mode & 1)
			selclear();
		else
			selstart(term.c.x, term.c.y, 0);
		set_notifmode(selectsearch_mode ^= 1, ksym);
		break;
	case XK_t:
		selextend(term.c.x, term.c.y, type ^= 3, i = 0);  /* 2 fois */
		selextend(term.c.x, term.c.y, type, i = 0);
		break;
	case XK_slash:
	case XK_KP_Divide:
	case XK_question:
		ksym &= XK_question;                /* Divide to slash */
		sens = (ksym == XK_slash) ? -1 : 1;
		ptarget = 0;
		set_notifmode(15, ksym);
		selectsearch_mode ^= 2;
		break;
	case XK_Escape:
		if (!in_use)
			break;
		selclear();
	case XK_Return:
		set_notifmode(4, ksym);
		term.c.x = cu.x, term.c.y = cu.y;
		select_or_drawcursor(selectsearch_mode = 0, type);
		in_use = quant = 0;
		return MODE_KBDSELECT;
	case XK_n:
	case XK_N:
		if (ptarget)
			search(selectsearch_mode, &target[0], ptarget, (ksym == XK_n) ? -1 : 1, type, &cu);
		break;
	case XK_BackSpace:
		term.c.x = 0;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	case XK_dollar:
		term.c.x = term.col - 1;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	case XK_Home:
		term.c.x = 0, term.c.y = 0;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	case XK_End:
		term.c.x = cu.x, term.c.y = cu.y;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	case XK_Page_Up:
	case XK_Page_Down:
		term.c.y = (ksym == XK_Prior ) ? 0 : cu.y;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	case XK_exclam:
		term.c.x = term.col >> 1;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	case XK_asterisk:
	case XK_KP_Multiply:
		term.c.x = term.col >> 1;
	case XK_underscore:
		term.c.y = cu.y >> 1;
		select_or_drawcursor(selectsearch_mode, type);
		break;
	default:
		if (ksym >= XK_0 && ksym <= XK_9) {                 /* 0-9 keyboard */
			quant = (quant * 10) + (ksym ^ XK_0);
			return 0;
		} else if (ksym >= XK_KP_0 && ksym <= XK_KP_9) {    /* 0-9 numpad */
			quant = (quant * 10) + (ksym ^ XK_KP_0);
			return 0;
		} else if (ksym == XK_k || ksym == XK_h)
			i = ksym & 1;
		else if (ksym == XK_l || ksym == XK_j)
			i = ((ksym & 6) | 4) >> 1;
		else if ((XK_Home & ksym) != XK_Home || (i = (ksym ^ XK_Home) - 1) > 3)
			break;

		xy = (i & 1) ? &term.c.y : &term.c.x;
		sens = (i & 2) ? 1 : -1;
		bound = (i >> 1 ^ 1) ? 0 : (i ^ 3) ? term.col - 1 : term.bot;

		if (quant == 0)
			quant++;

		if (*xy == bound && ((sens < 0 && bound == 0) || (sens > 0 && bound > 0)))
			break;

		*xy += quant * sens;
		if (*xy < 0 || ( bound > 0 && *xy > bound))
			*xy = bound;

		select_or_drawcursor(selectsearch_mode, type);
	}
	quant = 0;
	return 0;
}
