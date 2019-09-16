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
#define ALPHA_PATCH 1

/* This patch allows st to reize to any pixel size rather than snapping to character width/height.
 * https://st.suckless.org/patches/anysize/
 */
#define ANYSIZE_PATCH 1

/* By default bold text is rendered with a bold font in the bright variant of the current color.
 * This patch makes bold text rendered simply as bold, leaving the color unaffected.
 * https://st.suckless.org/patches/bold-is-not-bright/
 */
#define BOLD_IS_NOT_BRIGHT_PATCH 1

/* By default st only sets PRIMARY on selection.
 * This patch makes st set CLIPBOARD on selection.
 * https://st.suckless.org/patches/clipboard/
 */
#define CLIPBOARD_PATCH 1

/* Select and copy the last URL displayed with Mod+l. Multiple invocations cycle through the
 * available URLs.
 * https://st.suckless.org/patches/copyurl/
 */
#define COPYURL_PATCH 1

/* Select and copy the last URL displayed with Mod+l. Multiple invocations cycle through the
 * available URLs. This variant also highlights the selected URLs.
 * https://st.suckless.org/patches/copyurl/
 */
#define COPYURL_HIGHLIGHT_SELECTED_URLS_PATCH 1

/* This patch adds the option of disabling bold fonts globally.
 * https://st.suckless.org/patches/disable_bold_italic_fonts/
 */
#define DISABLE_BOLD_FONTS_PATCH 1

/* This patch adds the option of disabling italic fonts globally.
 * https://st.suckless.org/patches/disable_bold_italic_fonts/
 */
#define DISABLE_ITALIC_FONTS_PATCH 1

/* This patch adds the option of disabling roman fonts globally.
 * https://st.suckless.org/patches/disable_bold_italic_fonts/
 */
#define DISABLE_ROMAN_FONTS_PATCH 1

/* This patch adds better Input Method Editor (IME) support.
 * https://st.suckless.org/patches/fix_ime/
 */
#define FIXIME_PATCH 1

/* Hide the X cursor whenever a key is pressed and show it back when the mouse is moved in
 * the terminal window.
 * https://st.suckless.org/patches/hidecursor/
 */
#define HIDECURSOR_PATCH 1

/* This patch allows you to spawn a new st terminal using Ctrl-Shift-Return. It will have the
 * same CWD (current working directory) as the original st instance.
 * https://st.suckless.org/patches/newterm/
 */
#define NEWTERM_PATCH 1

/* Open contents of the clipboard in a user-defined browser.
 * https://st.suckless.org/patches/open_copied_url/
 */
#define OPENCOPIED_PATCH 1

/* This patch adds the ability to configure st via Xresources. At startup, st will read and
 * apply the resources named in the resources[] array in config.h.
 * https://st.suckless.org/patches/xresources/
 */
#define XRESOURCES_PATCH 1
