/* Patches */
#if ALPHA_PATCH
#include "alpha.c"
#endif
#if BACKGROUND_IMAGE_PATCH
#include "background_image_x.c"
#endif
#if BOXDRAW_PATCH
#include "boxdraw.c"
#endif
#if OPENCOPIED_PATCH
#include "opencopied.c"
#endif
#if FIXKEYBOARDINPUT_PATCH
#include "fixkeyboardinput.c"
#endif
#if FONT2_PATCH
#include "font2.c"
#endif
#if FULLSCREEN_PATCH
#include "fullscreen_x.c"
#endif
#if INVERT_PATCH
#include "invert.c"
#endif
#if REFLOW_PATCH && KEYBOARDSELECT_PATCH
#include "keyboardselect_reflow_x.c"
#elif KEYBOARDSELECT_PATCH
#include "keyboardselect_x.c"
#endif
#if NETWMICON_PATCH
#include "netwmicon.c"
#elif NETWMICON_FF_PATCH
#include "netwmicon_ff.c"
#elif NETWMICON_LEGACY_PATCH
#include "netwmicon_legacy.c"
#endif
#if OPENURLONCLICK_PATCH
#include "openurlonclick.c"
#endif
#if RIGHTCLICKTOPLUMB_PATCH
#include "rightclicktoplumb_x.c"
#endif
#if ST_EMBEDDER_PATCH
#include "st_embedder_x.c"
#endif
#if XRESOURCES_PATCH
#include "xresources.c"
#endif
#if OSC133_PATCH
#include "osc133.c"
#endif
