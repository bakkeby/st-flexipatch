#define TLINE(y) ((y) < term.scr ? term.hist[((y) + term.histi - \
                     term.scr + HISTSIZE + 1) % HISTSIZE] : \
                     term.line[(y) - term.scr])

#if COLUMNS_REFLOW_PATCH
#define TLINEABS(y) ( \
	(y) < 0 ? term.hist[(term.histi + (y) + 1 + HISTSIZE) % HISTSIZE] : term.line[(y)] \
)

enum scroll_mode {
	SCROLL_RESIZE = -1,
	SCROLL_NOSAVEHIST = 0,
	SCROLL_SAVEHIST = 1
};
#endif // COLUMNS_REFLOW_PATCH

void kscrolldown(const Arg *);
void kscrollup(const Arg *);

#if SCROLLBACK_MOUSE_PATCH || SCROLLBACK_MOUSE_ALTSCREEN_PATCH
typedef struct {
	 uint b;
	 uint mask;
	 void (*func)(const Arg *);
	 const Arg arg;
} MouseKey;

extern MouseKey mkeys[];
#endif // SCROLLBACK_MOUSE_PATCH / SCROLLBACK_MOUSE_ALTSCREEN_PATCH
