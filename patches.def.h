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

/* The alpha focus highlight patch allows the user to specify two distinct opacity values or
 * background colors in order to easily differentiate between focused and unfocused terminal
 * windows. This depends on the alpha patch.
 * https://github.com/juliusHuelsmann/st-focus/
 * https://st.suckless.org/patches/alpha_focus_highlight/
 */
#define ALPHA_FOCUS_HIGHLIGHT_PATCH 0

/* Adds gradient transparency to st, depends on the alpha patch.
 * https://st.suckless.org/patches/gradient/
 */
#define ALPHA_GRADIENT_PATCH 0

/* Allows for the initial size of the terminal to be specified as pixel width and height
 * using the -G command line option. Can be combined with the anysize patch to also allow
 * the window to be resized to any pixel size.
 * https://st.suckless.org/patches/anygeometry/
 */
#define ANYGEOMETRY_PATCH 0

/* This patch allows st to resize to any pixel size rather than snapping to character width/height.
 * https://st.suckless.org/patches/anysize/
 */
#define ANYSIZE_PATCH 0

/* A simple variant of the anysize patch that only changes the resize hints to allow the window to
 * be resized to any size.
 */
#define ANYSIZE_SIMPLE_PATCH 0

/* Draws a background image in farbfeld format in place of the defaultbg color allowing for pseudo
 * transparency.
 * https://st.suckless.org/patches/background_image/
 */
#define BACKGROUND_IMAGE_PATCH 0

/* This patch adds the ability to reload the background image config when a SIGUSR1 signal is
 * received, e.g.: killall -USR1 st
 * Depends on the BACKGROUND_IMAGE_PATCH.
 */
#define BACKGROUND_IMAGE_RELOAD_PATCH 0

/* This patch allows the use of a blinking cursor.
 * Only cursor styles 0, 1, 3, 5, and 7 blink. Set cursorstyle accordingly.
 * Cursor styles are defined here:
 *    https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps-SP-q.1D81
 * https://st.suckless.org/patches/blinking_cursor/
 */
#define BLINKING_CURSOR_PATCH 0

/* By default bold text is rendered with a bold font in the bright variant of the current color.
 * This patch makes bold text rendered simply as bold, leaving the color unaffected.
 * https://st.suckless.org/patches/bold-is-not-bright/
 */
#define BOLD_IS_NOT_BRIGHT_PATCH 0

/* This patch adds custom rendering of lines/blocks/braille characters for gapless alignment.
 * https://st.suckless.org/patches/boxdraw/
 */
#define BOXDRAW_PATCH 0

/* By default st only sets PRIMARY on selection.
 * This patch makes st set CLIPBOARD on selection.
 * https://st.suckless.org/patches/clipboard/
 */
#define CLIPBOARD_PATCH 0

/* This patch allows st to be resized without cutting off text when the terminal window is
 * made larger again. Text does not wrap when the terminal window is made smaller, you may
 * also want to have a look at the reflow patch.
 *
 * https://github.com/bakkeby/st-flexipatch/issues/34
 */
#define COLUMNS_PATCH 0

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

/* This patch adds support for CSI escape sequences 22 and 23, which save and
 * restores the window title (for instance nvim does this when opening and closing).
 * https://st.suckless.org/patches/csi_22_23/
 */
#define CSI_22_23_PATCH 0

/* According to the specification (see link in BLINKING_CURSOR_PATCH) the "Set cursor style
 * (DECSCUSR), VT520." escape sequences define both values of 0 and 1 as a blinking block,
 * with 1 being the default.
 *
 * This patch allows the default cursor to be set when value 0 is used, as opposed to
 * setting the cursor to a blinking block.
 *
 * This allows a command like this to restore the cursor to what st is configured with:
 *    $ echo -ne "\e[ q"
 *
 * While many terminal emulators do this it is not adhering to specification. xterm is an
 * example terminal that sets a blinking block instead of the configured one, same as st.
 */
#define DEFAULT_CURSOR_PATCH 0

/* Return BS on pressing backspace and DEL on pressing the delete key.
 * https://st.suckless.org/patches/delkey/
 */
#define DELKEY_PATCH 0

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

/* This patch adds the ability to toggle st into fullscreen mode.
 * Two key bindings are defined: F11 which is typical with other applications and Alt+Enter
 * which matches the default xterm behavior.
 * https://st.suckless.org/patches/fullscreen/
 */
#define FULLSCREEN_PATCH 0

/* Hide the X cursor whenever a key is pressed and show it back when the mouse is moved in
 * the terminal window.
 * https://st.suckless.org/patches/hidecursor/
 */
#define HIDECURSOR_PATCH 0

/* This patch hides the terminal cursor when the window loses focus (as opposed to showing a hollow
 * cursor).
 * https://www.reddit.com/r/suckless/comments/nvee8h/how_to_hide_cursor_in_st_is_there_a_patch_for_it/
 */
#define HIDE_TERMINAL_CURSOR_PATCH 0

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
 * NB: An improved variant of this patch is enabled if combined with the reflow patch.
 *
 * https://st.suckless.org/patches/keyboard_select/
 */
#define KEYBOARDSELECT_PATCH 0

/* This patch adds support for drawing ligatures using the Harfbuzz library to transform
 * original text of a single line to a list of glyphs with ligatures included.
 * This patch depends on the Harfbuzz library and headers to compile.
 * You need to uncomment the corresponding lines in config.mk to use the harfbuzz library
 * when including this patch.
 * https://github.com/cog1to/st-ligatures
 * https://st.suckless.org/patches/ligatures/
 */
#define LIGATURES_PATCH 0

/* This patch makes st ignore terminal color attributes by forcing display of the default
 * foreground and background colors only - making for a monochrome look. Idea ref.
 * https://www.reddit.com/r/suckless/comments/ixbx6z/how_to_use_black_and_white_only_for_st/
 */
#define MONOCHROME_PATCH 0

/* This patch sets the _NET_WM_ICON X property with an icon that is read from a .png file.
 * This patch depends on the GD Graphics Library and headers to compile.
 * You need to uncomment the corresponding lines in config.mk to use the gd library.
 *
 * The default location for the .png file is:
 *    - /usr/local/share/pixmaps/st.png
 *
 * https://st.suckless.org/patches/netwmicon/
 */
#define NETWMICON_PATCH 0

/* This patch sets the _NET_WM_ICON X property with an icon that is read from a farbfeld image.
 * The benefit of this patch is that you do not need an additional dependency on an external
 * library to read and convert the farbfeld image.
 *
 * The default location for the farbfeld image is:
 *    - /usr/local/share/pixmaps/st.ff
 *
 * Remember to change the ICONNAME in config.mk from st.png to st.ff when using this patch.
 *
 * Example command to convert a .png to farbfeld:
 *    $ png2ff < st.png > st.ff
 *
 * https://tools.suckless.org/farbfeld/
 * https://github.com/bakkeby/patches/wiki/netwmicon/
 */
#define NETWMICON_FF_PATCH 0

/* This patch sets the _NET_WM_ICON X property with a hardcoded icon for st. This is the
 * original version that predates the version that reads the image from a .png file.
 * https://st.suckless.org/patches/netwmicon/
 */
#define NETWMICON_LEGACY_PATCH 0

/* This patch allows you to spawn a new st terminal using Ctrl-Shift-Return. It will have the
 * same CWD (current working directory) as the original st instance.
 * https://st.suckless.org/patches/newterm/
 */
#define NEWTERM_PATCH 0

/* This patch will set the _MOTIF_WM_HINTS property for the st window which, if the window manager
 * respects it, will show the st window without window decorations.
 *
 * In dwm, if the decoration hints patch is applied, then the st window will start out without a
 * border. In GNOME and KDE the window should start without a window title.
 */
#define NO_WINDOW_DECORATIONS_PATCH 0

/* Open contents of the clipboard in a user-defined browser.
 * https://st.suckless.org/patches/open_copied_url/
 */
#define OPENCOPIED_PATCH 0

/* This patch allows for URLs to be opened directly when you click on them. This may not work with
 * all terminal applications.
 *
 * https://www.reddit.com/r/suckless/comments/cc83om/st_open_url/
 */
#define OPENURLONCLICK_PATCH 0

/* Reflow.
 * Allows st to be resized without cutting off text when the terminal window is made larger again.
 * Text wraps when the terminal window is made smaller.
 * Comes with scrollback.
 */
#define REFLOW_PATCH 0

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

/* This patch adds the two color-settings selectionfg and selectionbg to config.def.h.
 * Those define the fore- and background colors which are used when text on the screen is selected
 * with the mouse. This removes the default behaviour which would simply reverse the colors.
 * https://st.suckless.org/patches/selectioncolors/
 */
#define SELECTION_COLORS_PATCH 0

/* This is the single drawable buffer patch as outlined in the FAQ to get images
 * in w3m to display. While this patch does not break the alpha patch it images
 * are not shown in w3m if the alpha patch is applied.
 */
#define SINGLE_DRAWABLE_BUFFER_PATCH 0

/* This patch adds SIXEL graphics support for st.
 * Note that patch/sixel.c/sixel_hls.c come from mintty, licensed under GPL.
 * Known issues:
 *    - Rendering sixel graphics may cause unusual cursor placement, this is
 *      not specific to this variant of st - the same issue is present in
 *      the xterm implementation. This is likely an issue of sixel height
 *      not being detected correctly.
 *
 * Note that you need to uncomment the corresponding lines in config.mk when including this patch.
 * This patch is incompatible with the W3M patch.
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

/* This patch changes the mouse shape to the global default when the running program subscribes
 * for mouse events, for instance, in programs like ranger and fzf. It emulates the behaviour
 * shown by vte terminals like termite.
 * https://st.suckless.org/patches/swapmouse/
 */
#define SWAPMOUSE_PATCH 0

/* This patch adds synchronized-updates/application-sync support in st.
 * This will have no effect except when an application uses the synchronized-update escape
 * sequences. With this patch nearly all cursor flicker is eliminated in tmux, and tmux detects
 * it automatically via terminfo.
 *
 * Note: this patch alters st.info to promote support for extra escape sequences, which can
 * potentially cause application misbehaviour if you do not use this patch. Try removing or
 * commenting out the corresponding line in st.info if this is causing issues.
 *
 * https://st.suckless.org/patches/sync/
 */
#define SYNC_PATCH 0

/* Instead of a default X cursor, use the xterm cursor from your cursor theme.
 * You need to uncomment the corresponding line in config.mk to use the -lXcursor library
 * when including this patch.
 * https://st.suckless.org/patches/themed_cursor/
 */
#define THEMED_CURSOR_PATCH 0

/* Adds support for special underlines.
 *
 * Example test command:
 *    $ echo -e "\e[4:3m\e[58:5:10munderline\e[0m"
 *                  ^ ^     ^ ^  ^- sets terminal color 10
 *                  | |     |  \- indicates that terminal colors should be used
 *                  | |      \- indicates that underline color is being set
 *                  |  \- sets underline style to curvy
 *                   \- set underline
 *
 * Note: this patch alters st.info to promote support for extra escape sequences, which can
 * potentially cause application misbehaviour if you do not use this patch. Try removing or
 * commenting out the corresponding line in st.info if this is causing issues.
 *
 * https://st.suckless.org/patches/undercurl/
 */
#define UNDERCURL_PATCH 0

/* Allows mouse scroll without modifier keys for regardless of alt screen using the external
 * scroll program.
 * https://st.suckless.org/patches/universcroll/
 */
#define UNIVERSCROLL_PATCH 0

/* Use XftFontMatch in place of FcFontMatch.
 *
 * XftFontMatch calls XftDefaultSubstitute which configures various match properties according
 * to the user's configured Xft defaults (xrdb) as well as according to the current display and
 * screen. Most importantly, the screen DPI is computed [1]. Without this, st uses a "default"
 * DPI of 75 [2].
 *
 * [1]: https://cgit.freedesktop.org/xorg/lib/libXft/tree/src/xftdpy.c?id=libXft-2.3.2#n535
 * [2]: https://cgit.freedesktop.org/fontconfig/tree/src/fcdefault.c?id=2.11.1#n255
 *
 * https://git.suckless.org/st/commit/528241aa3835e2f1f052abeeaf891737712955a0.html
 */
#define USE_XFTFONTMATCH_PATCH 0

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
 * e.g.: killall -USR1 st
 * Depends on the XRESOURCES_PATCH.
 */
#define XRESOURCES_RELOAD_PATCH 0
