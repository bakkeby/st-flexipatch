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
		/* Note: this leaks memory */
		*sdst = strdup(ret.addr);
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

#if XRESOURCES_XDEFAULTS_PATCH
/* Returns an XrmDatabase that needs to be freed by the caller. */
static XrmDatabase
get_resources(Display *dpy)
{
	/*******************************************************************/
	/*  Adapted from rxvt-unicode-9.31 rxvttoolkit.C get_resources()  */
	/*******************************************************************/
	char *homedir = getenv("HOME");
	char fname[1024];

	char *displayResource, *xe;
	XrmDatabase rdb1;
	XrmDatabase database = XrmGetStringDatabase("");

	/* For ordering, see for example http://www.faqs.org/faqs/Xt-FAQ/ Subject: 20 */

	/* 6. System wide per application default file. */

	/* Add in $XAPPLRESDIR/St only; not bothering with XUSERFILESEARCHPATH */
	if ((xe = getenv("XAPPLRESDIR")) || (xe = "/etc/X11/app-defaults"))
	{
		snprintf(fname, sizeof(fname), "%s/%s", xe, "St");

		if ((rdb1 = XrmGetFileDatabase(fname)))
			XrmMergeDatabases(rdb1, &database);
	}

	/* 5. User's per application default file. None. */

	/* 4. User's defaults file. */
	if (homedir)
	{
		snprintf(fname, sizeof(fname), "%s/.Xdefaults", homedir);

		if ((rdb1 = XrmGetFileDatabase(fname)))
			XrmMergeDatabases(rdb1, &database);
	}

	/* Get any Xserver Resources (xrdb). */
	displayResource = XResourceManagerString(dpy);

	if (displayResource)
	{
		if ((rdb1 = XrmGetStringDatabase(displayResource)))
			XrmMergeDatabases(rdb1, &database);
	}

	/* Get screen specific resources. */
	displayResource = XScreenResourceString(ScreenOfDisplay(dpy, DefaultScreen(dpy)));

	if (displayResource)
	{
		if ((rdb1 = XrmGetStringDatabase(displayResource)))
			XrmMergeDatabases(rdb1, &database);

		XFree(displayResource);
	}

	/* 3. User's per host defaults file. */
	/* Add in XENVIRONMENT file */
	if ((xe = getenv("XENVIRONMENT")) && (rdb1 = XrmGetFileDatabase(xe)))
		XrmMergeDatabases(rdb1, &database);
	else if (homedir)
	{
		struct utsname un;

		if (!uname(&un))
		{
			snprintf(fname, sizeof(fname), "%s/.Xdefaults-%s", homedir, un.nodename);

			if ((rdb1 = XrmGetFileDatabase(fname)))
				XrmMergeDatabases(rdb1, &database);
		}
	}

	return database;
}

void
config_init(Display *dpy)
{
	XrmDatabase db;
	ResourcePref *p;

	XrmInitialize();
	db = get_resources(dpy);

	for (p = resources; p < resources + LEN(resources); p++)
		resource_load(db, p->name, p->type, p->dst);

	XrmDestroyDatabase(db);
}

#else // !XRESOURCES_XDEFAULTS_PATCH

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
	if (!db)
		return;

	for (p = resources; p < resources + LEN(resources); p++)
		resource_load(db, p->name, p->type, p->dst);

	XrmDestroyDatabase(db);
}
#endif // XRESOURCES_XDEFAULTS_PATCH

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
