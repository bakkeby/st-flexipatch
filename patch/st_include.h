/* Patches */
#if COPYURL_PATCH || COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH
#include "copyurl.h"
#endif
#if EXTERNALPIPE_PATCH
#include "externalpipe.h"
#endif
#if ISO14755_PATCH
#include "iso14755.h"
#endif
#if REFLOW_PATCH && KEYBOARDSELECT_PATCH
#include "keyboardselect_reflow_st.h"
#elif KEYBOARDSELECT_PATCH
#include "keyboardselect_st.h"
#endif
#if OPENURLONCLICK_PATCH
#include "openurlonclick.h"
#endif
#if RIGHTCLICKTOPLUMB_PATCH
#include "rightclicktoplumb_st.h"
#endif
#if NEWTERM_PATCH
#include "newterm.h"
#endif
#if REFLOW_PATCH
#include "reflow.h"
#elif SCROLLBACK_PATCH || SCROLLBACK_MOUSE_PATCH || SCROLLBACK_MOUSE_ALTSCREEN_PATCH
#include "scrollback.h"
#endif
#if SYNC_PATCH
#include "sync.h"
#endif
#if OSC7_PATCH
#include "osc7.h"
#endif
