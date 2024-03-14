int extpipeactive = 0;

void
#if EXTERNALPIPEIN_PATCH
extpipe(const Arg *arg, int in)
#else
externalpipe(const Arg *arg)
#endif // EXTERNALPIPEIN_PATCH
{
	int to[2];
	char buf[UTF_SIZ];
	void (*oldsigpipe)(int);
	Glyph *bp, *end;
	int lastpos, n, newline;

	if (pipe(to) == -1)
		return;

	switch (fork()) {
	case -1:
		close(to[0]);
		close(to[1]);
		return;
	case 0:
		dup2(to[0], STDIN_FILENO);
		close(to[0]);
		close(to[1]);
		#if EXTERNALPIPEIN_PATCH
		if (in)
			dup2(csdfd, STDOUT_FILENO);
		close(csdfd);
		#endif // EXTERNALPIPEIN_PATCH
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "st: execvp %s\n", ((char **)arg->v)[0]);
		perror("failed");
		exit(0);
	}

	close(to[0]);
	/* ignore sigpipe for now, in case child exists early */
	oldsigpipe = signal(SIGPIPE, SIG_IGN);
	newline = 0;
	for (n = 0; n < term.row; n++) {
		bp = term.line[n];
		#if REFLOW_PATCH
		lastpos = MIN(tlinelen(TLINE(n)) + 1, term.col) - 1;
		#else
		lastpos = MIN(tlinelen(n) + 1, term.col) - 1;
		#endif // REFLOW_PATCH
		if (lastpos < 0)
			break;
		end = &bp[lastpos + 1];
		for (; bp < end; ++bp)
			if (xwrite(to[1], buf, utf8encode(bp->u, buf)) < 0)
				break;
		if ((newline = term.line[n][lastpos].mode & ATTR_WRAP))
			continue;
		if (xwrite(to[1], "\n", 1) < 0)
			break;
		newline = 0;
	}
	if (newline)
		(void)xwrite(to[1], "\n", 1);
	close(to[1]);
	/* restore */
	signal(SIGPIPE, oldsigpipe);
	extpipeactive = 1;
}

#if EXTERNALPIPEIN_PATCH
void
externalpipe(const Arg *arg) {
	extpipe(arg, 0);
}

void
externalpipein(const Arg *arg) {
	extpipe(arg, 1);
}
#endif // EXTERNALPIPEIN_PATCH