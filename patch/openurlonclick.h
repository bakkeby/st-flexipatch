#include <spawn.h>

static inline void restoremousecursor(void) {
	if (!(win.mode & MODE_MOUSE) && xw.pointerisvisible)
		XDefineCursor(xw.dpy, xw.win, xw.vpointer);
}
static void clearurl(void);
static void openUrlOnClick(int col, int row, char* url_opener);
