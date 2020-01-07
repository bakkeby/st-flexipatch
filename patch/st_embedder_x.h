#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_FOCUS_CURRENT   0

static void createnotify(XEvent *e);
static void destroynotify(XEvent *e);
static void sendxembed(long msg, long detail, long d1, long d2);