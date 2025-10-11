float
clamp(float value, float lower, float upper) {
	if (value < lower)
		return lower;
	if (value > upper)
		return upper;
	return value;
}

void
changealpha(const Arg *arg)
{
	if ((alpha > 0 && arg->f < 0) || (alpha < 1 && arg->f > 0))
		alpha += arg->f;
	alpha = clamp(alpha, 0.0, 1.0);
	xloadcols();
	redraw();
}

#if ALPHA_FOCUS_HIGHLIGHT_PATCH
void
changealphaunfocused(const Arg *arg)
{
	if ((alphaUnfocused > 0 && arg->f < 0) || (alphaUnfocused < 1 && arg->f > 0))
		alphaUnfocused += arg->f;
	alphaUnfocused = clamp(alphaUnfocused, 0.0, 1.0);
	xloadcols();
	redraw();
}
#endif // ALPHA_FOCUS_HIGHLIGHT_PATCH
