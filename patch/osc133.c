void
scrolltoprompt(const Arg *arg)
{
	int x, y;
	#if REFLOW_PATCH
	int top = term.scr - term.histf;
	#else
	int top = term.scr - term.histn;
	#endif // REFLOW_PATCH
	int bot = term.scr + term.row-1;
	int dy = arg->i;
	Line line;

	if (!dy || tisaltscr())
		return;

	for (y = dy; y >= top && y <= bot; y += dy) {
		for (line = TLINE(y), x = 0; x < term.col; x++) {
			if (line[x].mode & ATTR_FTCS_PROMPT)
				goto scroll;
		}
	}

scroll:
	if (dy < 0)
		kscrollup(&((Arg){ .i = -y }));
	else
		kscrolldown(&((Arg){ .i = y }));
}
