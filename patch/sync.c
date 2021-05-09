#include <time.h>
struct timespec sutv;

static void
tsync_begin()
{
	clock_gettime(CLOCK_MONOTONIC, &sutv);
	su = 1;
}

static void
tsync_end()
{
	su = 0;
}

int
tinsync(uint timeout)
{
	struct timespec now;
	if (su && !clock_gettime(CLOCK_MONOTONIC, &now)
	       && TIMEDIFF(now, sutv) >= timeout)
		su = 0;
	return su;
}

int
ttyread_pending()
{
	return twrite_aborted;
}