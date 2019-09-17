/* Patches */

#if BOXDRAW_PATCH
#include "boxdraw.c"
#endif

#if OPENCOPIED_PATCH
#include "opencopied.c"
#endif

#if FIXIME_PATCH
#include "fixime.c"
#endif

#if FIXKEYBOARDINPUT_PATCH
#include "fixkeyboardinput.c"
#endif

#if KEYBOARDSELECT_PATCH
void toggle_winmode(int flag) {
        win.mode ^= flag;
}

void keyboard_select(const Arg *dummy) {
    win.mode ^= trt_kbdselect(-1, NULL, 0);
}
#endif // KEYBOARDSELECT_PATCH

#if RIGHTCLICKTOPLUMB_PATCH
#include "rightclicktoplumb_x.c"
#endif

#if VISUALBELL_2_PATCH || VISUALBELL_3_PATCH
#include "visualbell.c"
#endif

#if XRESOURCES_PATCH
#include "xresources.c"
#endif