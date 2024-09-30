/* Patches */
#if ALPHA_PATCH
#include "alpha.h"
#endif
#if BACKGROUND_IMAGE_PATCH
#include "background_image_x.h"
#endif
#if BOXDRAW_PATCH
#include "boxdraw.h"
#endif
#if OPENCOPIED_PATCH
#include "opencopied.h"
#endif
#if FONT2_PATCH
#include "font2.h"
#endif
#if FULLSCREEN_PATCH
#include "fullscreen_x.h"
#endif
#if INVERT_PATCH
#include "invert.h"
#endif
#if REFLOW_PATCH && KEYBOARDSELECT_PATCH
#include "keyboardselect_reflow_st.h"
#include "keyboardselect_reflow_x.h"
#elif KEYBOARDSELECT_PATCH
#include "keyboardselect_x.h"
#endif
#if NETWMICON_LEGACY_PATCH
#include "netwmicon_icon.h"
#endif
#if NETWMICON_PATCH || NETWMICON_FF_PATCH || NETWMICON_LEGACY_PATCH
#include "netwmicon.h"
#endif
#if RIGHTCLICKTOPLUMB_PATCH
#include "rightclicktoplumb_x.h"
#endif
#if ST_EMBEDDER_PATCH
#include "st_embedder_x.h"
#endif
#if XRESOURCES_PATCH
#include "xresources.h"
#endif
#if OSC133_PATCH
#include "osc133.h"
#endif
