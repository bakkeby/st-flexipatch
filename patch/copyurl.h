void copyurl(const Arg *);
#if COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH
static void tsetcolor(int, int, int, uint32_t, uint32_t);
static char * findlastany(char *, const char**, size_t);
#endif // COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH