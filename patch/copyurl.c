#if COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH
void
tsetcolor( int row, int start, int end, uint32_t fg, uint32_t bg )
{
	int i = start;
	for( ; i < end; ++i )
	{
		term.line[row][i].fg = fg;
		term.line[row][i].bg = bg;
	}
}

char *
findlastany(char *str, const char** find, size_t len)
{
	char* found = NULL;
	int i = 0;
	for(found = str + strlen(str) - 1; found >= str; --found) {
		for(i = 0; i < len; i++) {
			if(strncmp(found, find[i], strlen(find[i])) == 0) {
				return found;
			}
		}
	}

	return NULL;
}

/*
** Select and copy the previous url on screen (do nothing if there's no url).
**
** FIXME: doesn't handle urls that span multiple lines; will need to add support
**        for multiline "getsel()" first
*/
void
copyurl(const Arg *arg) {
	/* () and [] can appear in urls, but excluding them here will reduce false
	 * positives when figuring out where a given url ends.
	 */
	static char URLCHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789-._~:/?#@!$&'*+,;=%";

	static const char* URLSTRINGS[] = {"http://", "https://"};

	/* remove highlighting from previous selection if any */
	if(sel.ob.x >= 0 && sel.oe.x >= 0)
		tsetcolor(sel.nb.y, sel.ob.x, sel.oe.x + 1, defaultfg, defaultbg);

	int i = 0,
		row = 0, /* row of current URL */
		col = 0, /* column of current URL start */
		startrow = 0, /* row of last occurrence */
		colend = 0, /* column of last occurrence */
		passes = 0; /* how many rows have been scanned */

	char *linestr = calloc(term.col+1, sizeof(Rune));
	char *c = NULL,
		 *match = NULL;

	row = (sel.ob.x >= 0 && sel.nb.y > 0) ? sel.nb.y : term.bot;
	LIMIT(row, term.top, term.bot);
	startrow = row;

	colend = (sel.ob.x >= 0 && sel.nb.y > 0) ? sel.nb.x : term.col;
	LIMIT(colend, 0, term.col);

	/*
 	** Scan from (term.bot,term.col) to (0,0) and find
	** next occurrance of a URL
	*/
	while (passes !=term.bot + 2) {
		/* Read in each column of every row until
 		** we hit previous occurrence of URL
		*/
		for (col = 0, i = 0; col < colend; ++col,++i) {
			linestr[i] = term.line[row][col].u;
		}
		linestr[term.col] = '\0';

		if ((match = findlastany(linestr, URLSTRINGS,
						sizeof(URLSTRINGS)/sizeof(URLSTRINGS[0]))))
			break;

		if (--row < term.top)
			row = term.bot;

		colend = term.col;
		passes++;
	};

	if (match) {
		/* must happen before trim */
		selclear();
		sel.ob.x = strlen(linestr) - strlen(match);

		/* trim the rest of the line from the url match */
		for (c = match; *c != '\0'; ++c)
			if (!strchr(URLCHARS, *c)) {
				*c = '\0';
				break;
			}

		/* highlight selection by inverting terminal colors */
		tsetcolor(row, sel.ob.x, sel.ob.x + strlen( match ), defaultbg, defaultfg);

		/* select and copy */
		sel.mode = 1;
		sel.type = SEL_REGULAR;
		sel.oe.x = sel.ob.x + strlen(match)-1;
		sel.ob.y = sel.oe.y = row;
		selnormalize();
		tsetdirt(sel.nb.y, sel.ne.y);
		xsetsel(getsel());
		xclipcopy();
	}

	free(linestr);
}
#else
/* select and copy the previous url on screen (do nothing if there's no url).
 * known bug: doesn't handle urls that span multiple lines (wontfix), depends on multiline "getsel()"
 * known bug: only finds first url on line (mightfix)
 */
void
copyurl(const Arg *arg) {
	/* () and [] can appear in urls, but excluding them here will reduce false
	 * positives when figuring out where a given url ends.
	 */
	static char URLCHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789-._~:/?#@!$&'*+,;=%";

	int i, row, startrow;
	char *linestr = calloc(term.col+1, sizeof(Rune));
	char *c, *match = NULL;

	row = (sel.ob.x >= 0 && sel.nb.y > 0) ? sel.nb.y-1 : term.bot;
	LIMIT(row, term.top, term.bot);
	startrow = row;

	/* find the start of the last url before selection */
	do {
		for (i = 0; i < term.col; ++i) {
			linestr[i] = term.line[row][i].u;
		}
		linestr[term.col] = '\0';
		if ((match = strstr(linestr, "http://"))
				|| (match = strstr(linestr, "https://")))
			break;
		if (--row < term.top)
			row = term.bot;
	} while (row != startrow);

	if (match) {
		/* must happen before trim */
		selclear();
		sel.ob.x = strlen(linestr) - strlen(match);

		/* trim the rest of the line from the url match */
		for (c = match; *c != '\0'; ++c)
			if (!strchr(URLCHARS, *c)) {
				*c = '\0';
				break;
			}

		/* select and copy */
		sel.mode = 1;
		sel.type = SEL_REGULAR;
		sel.oe.x = sel.ob.x + strlen(match)-1;
		sel.ob.y = sel.oe.y = row;
		selnormalize();
		tsetdirt(sel.nb.y, sel.ne.y);
		xsetsel(getsel());
		xclipcopy();
	}

	free(linestr);
}
#endif // COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH