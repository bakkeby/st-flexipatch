/* Patches */

#if COPYURL_PATCH || COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH
#include "copyurl.h"
#endif

#if FIXIME_PATCH
void xximspot(int, int);
#endif

#if NEWTERM_PATCH
#include "newterm.h"
#endif