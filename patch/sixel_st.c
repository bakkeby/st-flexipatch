sixel_state_t sixel_st;

void
dcshandle(void)
{
	switch (csiescseq.mode[0]) {
	default:
		fprintf(stderr, "erresc: unknown csi ");
		csidump();
		/* die(""); */
		break;
	case 'q': /* DECSIXEL */
		if (sixel_parser_init(&sixel_st, 0, 0 << 16 | 0 << 8 | 0, 1, win.cw, win.ch) != 0)
			perror("sixel_parser_init() failed");
		term.mode |= MODE_SIXEL;
		break;
	}
}