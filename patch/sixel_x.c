void
delete_image(ImageList *im)
{
	if (im->prev)
		im->prev->next = im->next;
	else
		term.images = im->next;
	if (im->next)
		im->next->prev = im->prev;
	if (im->pixmap)
		XFreePixmap(xw.dpy, (Drawable)im->pixmap);
	free(im->pixels);
	free(im);
}