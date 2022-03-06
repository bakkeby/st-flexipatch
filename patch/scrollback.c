void
kscrolldown(const Arg* a)
{
	int n = a->i;

	if (n < 0)
		n = term.row + n;

	if (n > term.scr)
		n = term.scr;

	if (term.scr > 0) {
		term.scr -= n;
		selscroll(0, -n);
		tfulldirt();
	}

	#if SIXEL_PATCH
	scroll_images(-1*n);
	#endif // SIXEL_PATCH
}

void
kscrollup(const Arg* a)
{
	int n = a->i;
	if (n < 0)
		n = term.row + n;

	if (term.scr <= HISTSIZE-n) {
		term.scr += n;
		selscroll(0, n);
		tfulldirt();
	}

	if (term.scr > term.histi) {
		term.scr = term.histi;
	}
	else
	{
		#if SIXEL_PATCH
		scroll_images(n);
		#endif // SIXEL_PATCH
	}
}
