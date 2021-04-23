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
 * You need to uncomment the corresponding line in config.mk to use the -lXrender library
 * when including this patch.
 * https://st.suckless.org/patches/alpha/
 */
#define ALPHA_PATCH 0

/* Adds gradient transparency to st, depends on the alpha patch.
 * https://st.suckless.org/patches/gradient/
 */
#define ALPHA_GRADIENT_PATCH 0

/* This patch allows st to reize to any pixel size rather than snapping to character width/height.
 * https://st.suckless.org/patches/anysize/
 */
#define ANYSIZE_PATCH 0

/* This patch aims to prevent black bars being drawn on the edges of st terminals using the anysize
 * patch. This generally only occurs when the terminal background color doesn't match the colors
 * set in st's config.h file, for example when using terminal theming scripts such as base16.
 * (I have not found this to be working, but adding for reference. May reduce flickering on
 * terminal resizes.)
 * https://github.com/connor-brooks/st-anysize-nobar
 */
#define ANYSIZE_NOBAR_PATCH 0

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

/* This patch makes the cursor color the inverse of the current cell color.
 * https://st.suckless.org/patches/dynamic-cursor-color/
 */
#define DYNAMIC_CURSOR_COLOR_PATCH 0

/* Reading and writing st's screen through a pipe, e.g. pass info to dmenu.
 * https://st.suckless.org/patches/externalpipe/
 */
#define EXTERNALPIPE_PATCH 0

/* This patch improves and extends the externalpipe patch in two ways:
 *    - it prevents the reset of the signal handler set on SIGCHILD, when
 *      the forked process that executes the external process exits and
 *    - it adds the externalpipein function to redirect the standard output
 *      of the external command to the slave size of the pty, that is, as if
 *      the external program had been manually executed on the terminal
 *
 * It can be used to send desired escape sequences to the terminal with a
 * keyboard shortcut. The patch was created to make use of the dynamic-colors
 * tool that uses the OSC escape sequences to change the colors of the terminal.
 *
 * This patch depends on EXTERNALPIPE_PATCH being enabled.
 *
 * https://github.com/sos4nt/dynamic-colors
 * https://lists.suckless.org/hackers/2004/17218.html
 */
#define EXTERNALPIPEIN_PATCH 0

/* This patch allows command line applications to use all the fancy key combinations
 * that are available to GUI applications.
 * https://st.suckless.org/patches/fix_keyboard_input/
 */
#define FIXKEYBOARDINPUT_PATCH 0

/* This patch allows you to add spare font besides the default. Some glyphs can be not present in
 * the default font. For this glyphs st uses font-config and try to find them in font cache first.
 * This patch append fonts defined in font2 variable to the beginning of the font cache.
 * So they will be used first for glyphs that are absent in the default font.
 * https://st.suckless.org/patches/font2/
 */
#define FONT2_PATCH 0

/* Hide the X cursor whenever a key is pressed and show it back when the mouse is moved in
 * the terminal window.
 * https://st.suckless.org/patches/hidecursor/
 */
#define HIDECURSOR_PATCH 0

/* This patch adds a keybinding that lets you invert the current colorscheme of st.
 * This provides a simple way to temporarily switch to a light colorscheme if you use a dark
 * colorscheme or visa-versa.
 * https://st.suckless.org/patches/invert/
 */
#define INVERT_PATCH 0

/* Pressing the default binding Ctrl+Shift-i will popup dmenu, asking you to enter a unicode
 * codepoint that will be converted to a glyph and then pushed to st.
 * https://st.suckless.org/patches/iso14755/
 */
#define ISO14755_PATCH 0

/* This patch allows you to select text on the terminal using keyboard shortcuts.
 * https://st.suckless.org/patches/keyboard_select/
 */
#define KEYBOARDSELECT_PATCH 0

/* This patch adds support for drawing ligatures using the Harfbuzz library to transform
 * original text of a single line to a list of glyphs with ligatures included.
 * This patch depends on the Harfbuzz library and headers to compile.
 * You need to uncomment the corresponding line in config.mk to use the harfbuzz library
 * when including this patch.
 * You need to uncomment the corresponding lines in Makefile when including this patch.
 * https://github.com/cog1to/st-ligatures
 * https://st.suckless.org/patches/ligatures/
 */
#define LIGATURES_PATCH 0

/* This patch makes st ignore terminal color attributes by forcing display of the default
 * foreground and background colors only - making for a monochrome look. Idea ref.
 * https://www.reddit.com/r/suckless/comments/ixbx6z/how_to_use_black_and_white_only_for_st/
 */
#define MONOCHROME_PATCH 0

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

/* This is the single drawable buffer patch as outlined in the FAQ to get images
 * in w3m to display. While this patch does not break the alpha patch it images
 * are not shown in w3m if the alpha patch is applied.
 */
#define SINGLE_DRAWABLE_BUFFER_PATCH 0

/* This patch adds SIXEL graphics support for st.
 * Note that patch/sixel.c/sixel_hls.c come from mintty, licensed under GPL.
 * Known issues:
 *    - Entering clear causes all sixels to be deleted from scrollback.
 *    - Rendering sixel graphics may cause unusual cursor placement, this is
 *      not specific to this variant of st - the same issue is present in
 *      the xterm implementation. This is likely an issue of sixel height
 *      not being detected correctly.
 *    - If combined with the alpha patch sixel graphics disappear (become white)
 *      when transparent and rendered against a white background. This is believed
 *      to be related to how the sixel graphics use RGB colors instead of RGBA.
 *      A pull request or instructions for how to properly add alpha support for
 *      sixel graphics would be very welcome.
 *
 * Note that you need to uncomment the corresponding lines in Makefile when including this patch.
 *
 * https://gist.github.com/saitoha/70e0fdf22e3e8f63ce937c7f7da71809
 */
#define SIXEL_PATCH 0

/* This patch allows clients to embed into the st window and is useful if you tend to
 * start X applications from the terminal. For example:
 *
 *   $ surf -e $WINDOWID
 *
 * The behavior is similar to Plan 9 where applications can take over windows.
 * URL TBC
 */
#define ST_EMBEDDER_PATCH 0

/* Use inverted defaultbg/fg for selection when bg/fg are the same.
 * https://st.suckless.org/patches/spoiler/
 */
#define SPOILER_PATCH 0

/* Instead of a default X cursor, use the xterm cursor from your cursor theme.
 * You need to uncomment the corresponding line in config.mk to use the -lXcursor library
 * when including this patch.
 * https://st.suckless.org/patches/themed_cursor/
 */
#define THEMED_CURSOR_PATCH 0

/* Vertically center lines in the space available if you have set a larger chscale in config.h
 * https://st.suckless.org/patches/vertcenter/
 */
#define VERTCENTER_PATCH 0

/* Briefly inverts window content on terminal bell event.
 * https://st.suckless.org/patches/visualbell/
 */
#define VISUALBELL_1_PATCH 0

/* Adds support for w3m images.
 * https://st.suckless.org/patches/w3m/
 */
#define W3M_PATCH 0

/* Adds proper glyphs rendering in st allowing wide glyphs to be drawn as-is as opposed to
 * smaller or cut glyphs being rendered.
 * https://github.com/Dreomite/st/commit/e3b821dcb3511d60341dec35ee05a4a0abfef7f2
 * https://www.reddit.com/r/suckless/comments/jt90ai/update_support_for_proper_glyph_rendering_in_st/
 */
#define WIDE_GLYPHS_PATCH 0

/* There is a known issue that Google's Variable Fonts (VF) can end up with letter spacing
 * that is too wide in programs that use Xft, for example Inconsolata v3.000.
 *
 * This is intended as a temporary patch / hack until (if) this is fixed in the Xft library
 * itself.
 *
 * https://github.com/googlefonts/Inconsolata/issues/42#issuecomment-737508890
 */
#define WIDE_GLYPH_SPACING_PATCH 0

/* This patch allows user to specify the initial path st should use as the working directory.
 * https://st.suckless.org/patches/workingdir/
 */
#define WORKINGDIR_PATCH 0

/* This patch adds the ability to configure st via Xresources. At startup, st will read and
 * apply the resources named in the resources[] array in config.h.
 * https://st.suckless.org/patches/xresources/
 */
#define XRESOURCES_PATCH 0

/* This patch adds the ability to reload the Xresources config when a SIGUSR1 signal is received
  e.g.: killall -USR1 st
  Depends on the XRESOURCES_PATCH.
 */
#define XRESOURCES_RELOAD_PATCH 0
