int
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char **sdst = dst;
	int *idst = dst;
	float *fdst = dst;

	char fullname[256];
	char fullclass[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s",
			opt_name ? opt_name : "st", name);
	snprintf(fullclass, sizeof(fullclass), "%s.%s",
			opt_class ? opt_class : "St", name);
	fullname[sizeof(fullname) - 1] = fullclass[sizeof(fullclass) - 1] = '\0';

	XrmGetResource(db, fullname, fullclass, &type, &ret);
	if (ret.addr == NULL || strncmp("String", type, 64))
		return 1;

	switch (rtype) {
	case STRING:
		*sdst = ret.addr;
		break;
	case INTEGER:
		*idst = strtoul(ret.addr, NULL, 10);
		break;
	case FLOAT:
		*fdst = strtof(ret.addr, NULL);
		break;
	}
	return 0;
}

void
config_init(Display *dpy)
{
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	XrmInitialize();
	resm = XResourceManagerString(dpy);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LEN(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
}

#if XRESOURCES_RELOAD_PATCH
void
reload_config(int sig)
{
	/* Recreate a Display object to have up to date Xresources entries */
	Display *dpy;
	if (!(dpy = XOpenDisplay(NULL)))
		die("Can't open display\n");

	config_init(dpy);
	xloadcols();

	/* nearly like zoomabs() */
	xunloadfonts();
	xloadfonts(font, 0); /* font <- config_init() */
	#if FONT2_PATCH
	xloadsparefonts();
	#endif // FONT2_PATCH
	cresize(0, 0);
	redraw();
	xhints();

	XCloseDisplay(dpy);
}
#endif // XRESOURCES_RELOAD_PATCH
