static int tiswrapped(Line line);
static char *tgetline(char *, const Glyph *, const Glyph *, int);

static void tclearglyph(Glyph *, int);
static void tresetcursor(void);
static void twritetab(void);

static void tloaddefscreen(int, int);
static void tloadaltscreen(int, int);
static void treflow(int, int);
static void rscrolldown(int);
static void tresizedef(int, int);
static void tresizealt(int, int);

static void selmove(int);
static void selremove(void);
static int regionselected(int, int, int, int);