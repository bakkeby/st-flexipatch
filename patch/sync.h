static int su = 0;
static int twrite_aborted = 0;

static void tsync_begin();
static void tsync_end();
int tinsync(uint timeout);
int ttyread_pending();