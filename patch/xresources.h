#include <X11/Xresource.h>

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

int resource_load(XrmDatabase, char *, enum resource_type, void *);
#if XRESOURCES_RELOAD_PATCH
void config_init(Display *dpy);
#else
void config_init(void);
#endif // XRESOURCES_RELOAD_PATCH
