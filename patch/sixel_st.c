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

void
scroll_images(int n) {
	ImageList *im;
	int tmp;

	/* maximum sixel distance in lines from current view before
	 * deallocation
	 * TODO: should be in config.h */
	int max_sixel_distance = 10000;

	for (im = term.images; im; im = im->next) {
		im->y += n;

		/* check if the current sixel has exceeded the maximum
		 * draw distance, and should therefore be deleted */
		tmp = im->y;
		if (tmp < 0) { tmp = tmp * -1; }
		if (tmp > max_sixel_distance) {
			fprintf(stderr, "im@0x%08x exceeded maximum distance\n");
			im->should_delete = 1;
		}
	}
}