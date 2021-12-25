void
kscrolldown(const Arg* a)
{
	int n = a->i;

	#if COLUMNS_REFLOW_PATCH
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
	#else
	if (n < 0)
		n = term.row + n;

	if (n > term.scr)
		n = term.scr;

	if (term.scr > 0) {
		term.scr -= n;
		selscroll(0, -n);
		tfulldirt();
	}
	#endif // COLUMNS_REFLOW_PATCH

	#if SIXEL_PATCH
	scroll_images(-1*n);
	#endif // SIXEL_PATCH
}

void
kscrollup(const Arg* a)
{
	int n = a->i;

	#if COLUMNS_REFLOW_PATCH
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
	#else
	if (n < 0)
		n = term.row + n;

	if (term.scr <= HISTSIZE-n) {
		term.scr += n;
		selscroll(0, n);
		tfulldirt();
	}
	#endif // COLUMNS_REFLOW_PATCH

	#if SIXEL_PATCH
	scroll_images(n);
	#endif // SIXEL_PATCH
}
