/*
 * This file contains patch control flags.
 *
 * In principle you should be able to mix and match any patches
 * you may want. In cases where patches are logically incompatible
 * one patch may take precedence over the other as noted in the
 * relevant descriptions.
 */

/* Patches */

/* The alpha patch adds transparency for the terminal.
 * https://st.suckless.org/patches/alpha/
 */
#define ALPHA_PATCH 0

/* This patch allows st to reize to any pixel size rather than snapping to character width/height.
 * https://st.suckless.org/patches/anysize/
 */
#define ANYSIZE_PATCH 0

/* By default bold text is rendered with a bold font in the bright variant of the current color.
 * This patch makes bold text rendered simply as bold, leaving the color unaffected.
 * https://st.suckless.org/patches/bold-is-not-bright/
 */
#define BOLD_IS_NOT_BRIGHT_PATCH 0

/* This patch adds dustom rendering of lines/blocks/braille characters for gapless alignment.
 * https://st.suckless.org/patches/boxdraw/
 */
#define BOXDRAW_PATCH 0

/* By default st only sets PRIMARY on selection.
 * This patch makes st set CLIPBOARD on selection.
 * https://st.suckless.org/patches/clipboard/
 */
#define CLIPBOARD_PATCH 0

/* Select and copy the last URL displayed with Mod+l. Multiple invocations cycle through the
 * available URLs.
 * https://st.suckless.org/patches/copyurl/
 */
#define COPYURL_PATCH 0

/* Select and copy the last URL displayed with Mod+l. Multiple invocations cycle through the
 * available URLs. This variant also highlights the selected URLs.
 * https://st.suckless.org/patches/copyurl/
 */
#define COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH 0

/* This patch adds the option of disabling bold fonts globally.
 * https://st.suckless.org/patches/disable_bold_italic_fonts/
 */
#define DISABLE_BOLD_FONTS_PATCH 0

/* This patch adds the option of disabling italic fonts globally.
 * https://st.suckless.org/patches/disable_bold_italic_fonts/
 */
#define DISABLE_ITALIC_FONTS_PATCH 0

/* This patch adds the option of disabling roman fonts globally.
 * https://st.suckless.org/patches/disable_bold_italic_fonts/
 */
#define DISABLE_ROMAN_FONTS_PATCH 0

/* Reading and writing st's screen through a pipe, e.g. pass info to dmenu.
 * https://st.suckless.org/patches/externalpipe/
 */
#define EXTERNALPIPE_PATCH 0

/* This patch adds better Input Method Editor (IME) support.
 * https://st.suckless.org/patches/fix_ime/
 */
#define FIXIME_PATCH 0

/*
 * https://st.suckless.org/patches/fix_keyboard_input/
 */
#define FIXKEYBOARDINPUT_PATCH 0

/* Hide the X cursor whenever a key is pressed and show it back when the mouse is moved in
 * the terminal window.
 * https://st.suckless.org/patches/hidecursor/
 */
#define HIDECURSOR_PATCH 0

/* Pressing the default binding Ctrl+Shift-i will popup dmenu, asking you to enter a unicode
 * codepoint that will be converted to a glyph and then pushed to st.
 * https://st.suckless.org/patches/iso14755/
 */
#define ISO14755_PATCH 0

/* This patch allows you to select text on the terminal using keyboard shortcuts.
 * https://st.suckless.org/patches/keyboard_select/
 */
#define KEYBOARDSELECT_PATCH 0

/* This patch allows you to spawn a new st terminal using Ctrl-Shift-Return. It will have the
 * same CWD (current working directory) as the original st instance.
 * https://st.suckless.org/patches/newterm/
 */
#define NEWTERM_PATCH 0

/* Open contents of the clipboard in a user-defined browser.
 * https://st.suckless.org/patches/open_copied_url/
 */
#define OPENCOPIED_PATCH 0

/* This patch allows you to specify a border that is relative in size to the width of a cell
 * in the terminal.
 * https://st.suckless.org/patches/relativeborder/
 */
#define RELATIVEBORDER_PATCH 0

/* This patch allows you to right-click on some selected text to send it to the plumbing
 * program of choice, e.g. open a file, view an image, open a URL.
 * https://st.suckless.org/patches/right_click_to_plumb/
 */
#define RIGHTCLICKTOPLUMB_PATCH 0

/* Scroll back through terminal output using Shift+{PageUp, PageDown}.
 * https://st.suckless.org/patches/scrollback/
 */
#define SCROLLBACK_PATCH 0

/* Scroll back through terminal output using Shift+MouseWheel.
 * This variant depends on SCROLLBACK_PATCH being enabled.
 * https://st.suckless.org/patches/scrollback/
 */
#define SCROLLBACK_MOUSE_PATCH 0

/* Scroll back through terminal output using mouse wheel (when not in MODE_ALTSCREEN).
 * This variant depends on SCROLLBACK_PATCH being enabled.
 * https://st.suckless.org/patches/scrollback/
 */
#define SCROLLBACK_MOUSE_ALTSCREEN_PATCH 0

/* Use inverted defaultbg/fg for selection when bg/fg are the same.
 * https://st.suckless.org/patches/spoiler/
 */
#define SPOILER_PATCH 0

/* Instead of a default X cursor, use the xterm cursor from your cursor theme.
 * https://st.suckless.org/patches/themed_cursor/
 */
#define THEMED_CURSOR_PATCH 0

/*
 * Vertically center lines in the space available if you have set a larger chscale in config.h
 * https://st.suckless.org/patches/vertcenter/
 */
#define VERTCENTER_PATCH 0

/* On receiving a terminal bell event this patch briefly inverts the window content colors.
 * You may need to reduce the xfps value in config.h to less or equal to that of the refresh
 * rate of your monitor for this to be noticeble.
 * The visualbell 2 and 3 patches takes precedence over this patch.
 * https://st.suckless.org/patches/visualbell/
 */
#define VISUALBELL_1_PATCH 0

/* On receiving a terminal bell event this patch either:
 *    - briefly inverts the window content colors across the whole terminal or
 *    - briefly inverts the window content colors across the window border
 * The visualbell 3 patch takes precedence over this patch.
 * https://st.suckless.org/patches/visualbell/
 */
#define VISUALBELL_2_PATCH 0

/* On receiving a terminal bell event this patch either:
 *    - briefly inverts the window content colors across the whole terminal or
 *    - briefly inverts the window content colors across the window border or
 *    - draws a (configurable) circle as a visual bell indicator
 * https://st.suckless.org/patches/visualbell/
 */
#define VISUALBELL_3_PATCH 0

/* This patch adds the ability to configure st via Xresources. At startup, st will read and
 * apply the resources named in the resources[] array in config.h.
 * https://st.suckless.org/patches/xresources/
 */
#define XRESOURCES_PATCH 0
