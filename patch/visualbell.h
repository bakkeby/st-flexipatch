static void vbellbegin();
static int isvbellcell(int x, int y);

#if VISUALBELL_3_PATCH
static void xdrawvbell();
static void xfillcircle(int x, int y, int r, uint color_ix);
#endif // VISUALBELL_3_PATCH