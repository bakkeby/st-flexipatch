#define TLINE(y) ((y) < term.scr ? term.hist[((y) + term.histi - \
                     term.scr + HISTSIZE + 1) % HISTSIZE] : \
                     term.line[(y) - term.scr])

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
