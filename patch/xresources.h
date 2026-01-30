#include <X11/Xresource.h>
#if XRESOURCES_XDEFAULTS_PATCH
#include <sys/utsname.h>
#endif // XRESOURCES_XDEFAULTS_PATCH

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
void config_init(Display *dpy);
