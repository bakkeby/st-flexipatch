float
clamp(float value, float lower, float upper) {
    if(value < lower)
        return lower;
    if(value > upper)
        return upper;
    return value;
}

void
changealpha(const Arg *arg)
{
    if((alpha > 0 && arg->f < 0) || (alpha < 1 && arg->f > 0))
        alpha += arg->f;
    alpha = clamp(alpha, 0.0, 1.0);
    #if ALPHA_FOCUS_HIGHLIGHT_PATCH
    alphaUnfocus = clamp(alpha, 0.0, 1.0);
    #endif
    xloadcols();
    redraw();
}
