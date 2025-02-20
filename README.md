Similar to [dwm-flexipatch](https://github.com/bakkeby/dwm-flexipatch) this st 0.9.2 (6009e6e, 2024-11-25) project has a different take on st patching. It uses preprocessor directives to decide whether or not to include a patch during build time. Essentially this means that this build, for better or worse, contains both the patched _and_ the original code. The aim being that you can select which patches to include and the build will contain that code and nothing more.

For example to include the `alpha` patch then you would only need to flip this setting from 0 to 1 in [patches.h](https://github.com/bakkeby/st-flexipatch/blob/master/patches.def.h):
```c
#define ALPHA_PATCH 1
```

Once you have found out what works for you and what doesn't then you should be in a better position to choose patches should you want to start patching from scratch.

Alternatively if you have found the patches you want, but don't want the rest of the flexipatch entanglement on your plate then you may want to have a look at [flexipatch-finalizer](https://github.com/bakkeby/flexipatch-finalizer); a custom pre-processor tool that removes all the unused flexipatch code leaving you with a build that contains the patches you selected.

Refer to [https://st.suckless.org/](https://st.suckless.org/) for details on the st terminal, how to install it and how it works.

---

### Changelog:

2025-02-20 - Added the drag-n-drop and open-selected-text patches

2024-05-31 - Added the anygeometry patch

2024-03-13 - Added the reflow patch and upgraded the netwmicon patch

2024-03-07 - Improved sixel support, removed VIM browse patch

2022-10-24 - Added the fullscreen patch

2022-08-28 - Added the use XftFontMatch patch

2022-08-24 - Added the no window decorations patch

2022-04-11 - Added the background image reload patch

2022-03-10 - Added the background image patch

2022-02-24 - Upgraded to st 0.8.5 e823e23, 2022-02-17 - removing osc_10_11_12_2 patch as no longer relevant

2021-08-18 - Added the CSI 22 & 23 patch

2021-07-26 - Added columns patch

2021-07-07 - Added sixel scrollback and the openurlonclick patch

2021-06-09 - Added the hide terminal cursor patch

2021-05-16 - Added swapmouse patch

2021-05-11 - Added default cursor patch

2021-05-10 - Upgrade to 46b02f, 2021-03-28

2021-05-09 - Added the sync, alpha-focus-hightlight and vim browse patches

2021-05-08 - Added blinking cursor, delkey, undercurl,universcroll, desktopentry, netwmicon and osc_10_11_12_2 patches

2021-05-07 - Added xresources reload patch

2021-04-21 - Added (temporary?) hack for Variable Fonts (VT) support

2021-03-10 - Added sixel support

2021-02-26 - Added the dynamic cursor color patch

2021-02-15 - Added the alpha gradient patch

2020-11-14 - Added the wide glyphs patch

2020-10-23 - Added the monochrome patch

2020-08-08 - Re-added the visualbell patch

2020-06-26 - Added the single drawable buffer patch as per the FAQ in order to get w3m images to display

2020-06-25 - Upgrade to 0.8.4 (367803, 2020-06-19)

2020-06-14 - Added w3m patch

2020-06-10 - Upgrade to 249ef9, 2020-06-01

2020-06-05 - Added the ligatures patch

2020-05-20 - Upgrade to 222876, 2020-05-09, and removed visualbell 1, 2, 3 patches and force redraw after keypress due to incompatibility. Refer to tag [371878](https://github.com/bakkeby/st-flexipatch/tree/371878) if you want to try these out.

2020-04-20 - Upgrade to c279f5, 2020-04-19, and added the force redraw on pselect after key is pressed patch and the externalpipein patch

2020-03-29 - Added invert and workingdir patches

2020-03-24 - Upgraded to latest (master) of st (commit 51e19ea11dd42eefed1ca136ee3f6be975f618b1 at the time of writing). Custom changes to make the altscreen mouse scollback patch working.

2020-03-21 - Added font2 patch

2020-01-07 - Added st embedder patch

2019-10-16 - Introduced [flexipatch-finalizer](https://github.com/bakkeby/flexipatch-finalizer)

2019-09-17 - Added relativeborder, fix-keyboard-input, iso14755, visualbell, right-click-to-plumb, boxdraw and keyboard-select patches

2019-09-16 - Added alpha, anysize, bold-is-not-bright, clipboard, copyurl, disable-fonts, externalpipe, fixime, hidecursor, newterm, open-copied-url, vertcenter, scrollback, spoiler, themed cursor and xresources patches

### Patches included:

   - [alpha](https://st.suckless.org/patches/alpha/)
      - adds transparency for the terminal

   - [alpha-focus-highlight](https://st.suckless.org/patches/alpha_focus_highlight/)
      - allows the user to specify two distinct opacity values or background colors in order to
        easily differentiate between focused and unfocused terminal windows

   - [anygeometry](https://st.suckless.org/patches/anygeometry/)
      - allows st to start at any pixel size using the \-G command line option (if floating)
      - can be combined with the anysize patch to resize to any pixel size

   - [anysize](https://st.suckless.org/patches/anysize/)
      - allows st to reize to any pixel size rather than snapping to character width / height

   - [~anysize\_nobar~](https://github.com/connor-brooks/st-anysize-nobar)
      - ~a patch that aims to prevent black bars being drawn on the edges of st terminals using the
        anysize patch~

   - [background-image](https://st.suckless.org/patches/background_image/)
      - draws a background image in farbfeld format in place of the defaultbg color allowing for
        pseudo transparency

   - background-image-reload
      - allows the background image to be reloaded similar to xresources using USR1 signals

   - [blinking-cursor](https://st.suckless.org/patches/blinking_cursor/)
      - allows the use of a blinking cursor

   - [bold-is-not-bright](https://st.suckless.org/patches/bold-is-not-bright/)
      - by default bold text is rendered with a bold font in the bright variant of the current color
      - this patch makes bold text rendered simply as bold, leaving the color unaffected

   - [boxdraw](https://st.suckless.org/patches/boxdraw/)
      - adds dustom rendering of lines/blocks/braille characters for gapless alignment

   - [clipboard](https://st.suckless.org/patches/clipboard/)
      - by default st only sets PRIMARY on selection
      - this patch makes st set CLIPBOARD on selection

   - [columns](https://github.com/bakkeby/st-flexipatch/issues/34)
      - allows st to be resized without cutting off text when the terminal window is made larger again
      - text does not wrap when the terminal window is made smaller

   - [copyurl](https://st.suckless.org/patches/copyurl/)
      - this patch allows you to select and copy the last URL displayed with Mod+l
      - multiple invocations cycle through the available URLs

   - [csi\_23\_23](https://st.suckless.org/patches/csi_22_23/)
      - adds support for CSI escape sequences 22 and 23, which save and restores the window title
        (for instance nvim does this when opening and closing)

   - default-cursor
      - minor change allowing escape sequences like `\e[ q` or `\e[0 q` to set the cursor back to default configuration instead of a blinking block
      - while many terminals do this the behaviour is not according to the specification

   - [delkey](https://st.suckless.org/patches/delkey/)
      - return BS on pressing backspace and DEL on pressing the delete key

   - [desktopentry](https://st.suckless.org/patches/desktopentry/)
      - adds a desktop entry for st so that it can be displayed with an icon when using a graphical launcher
      - this patch only applies to the Makefile and is enabled by default, remove if not needed

   - [disable-fonts](https://st.suckless.org/patches/disable_bold_italic_fonts/)
      - this patch adds the option of disabling bold/italic/roman fonts globally

   - [drag-n-drop](https://st.suckless.org/patches/drag-n-drop)
      - allows dragging a file into the terminal and have the path printed

   - [dynamic-cursor-color](https://st.suckless.org/patches/dynamic-cursor-color/)
      - this patch makes the cursor color the inverse of the current cell color

   - [externalpipe](https://st.suckless.org/patches/externalpipe/)
      - this patch allows for reading and writing st's screen through a pipe, e.g. to pass info to dmenu

   - [externalpipein](https://lists.suckless.org/hackers/2004/17218.html)
      - this patch prevents the reset of the signal handler set on SIGCHILD, when the forked process that executes the external process exits
      - it adds the externalpipein function to redirect the standard output of the external command to the slave size of the pty, that is, as if the external program had been manually executed on the terminal
      - this can be used to send desired escape sequences to the terminal with a shortcut (e.g. to change colors)

   - [~fixime~](https://st.suckless.org/patches/fix_ime/)
      - adds better Input Method Editor (IME) support
      - (included in the base as per [35f7db](https://git.suckless.org/st/commit/e85b6b64660214121164ea97fb098eaa4935f7db.html))

   - [fix-keyboard-input](https://st.suckless.org/patches/fix_keyboard_input/)
      - allows cli applications to use all the fancy key combinations that are available to GUI applications

   - [font2](https://st.suckless.org/patches/font2/)
      - allows you to add a spare font besides the default

   - [~force-redraw-after-keypress~](https://lists.suckless.org/hackers/2004/17221.html)
      - ~this patch forces the terminal to check for new data on the tty on keypress with the aim of reducing input latency~

   - [fullscreen](https://st.suckless.org/patches/fullscreen/)
      - allows the st window to go into fullscreen mode

   - [gradient](https://st.suckless.org/patches/gradient/)
      - adds gradient transparency to st
      - depends on the alpha patch

   - [hidecursor](https://st.suckless.org/patches/hidecursor/)
      - hides the X cursor whenever a key is pressed and show it back when the mouse is moved in the terminal window

   - [hide-terminal-cursor](https://www.reddit.com/r/suckless/comments/nvee8h/how_to_hide_cursor_in_st_is_there_a_patch_for_it/)
      - hides the terminal cursor when the window loses focus (as opposed to showing a hollow cursor)

   - [invert](https://st.suckless.org/patches/invert/)
      - adds a keybinding that lets you invert the current colorscheme of st
      - this provides a simple way to temporarily switch to a light colorscheme if you use a dark colorscheme or visa-versa

   - [iso14755](https://st.suckless.org/patches/iso14755/)
      - pressing the default binding Ctrl+Shift-i will popup dmenu, asking you to enter a unicode codepoint that will be converted to a glyph and then pushed to st

   - [keyboard-select](https://st.suckless.org/patches/keyboard_select/)
      - allows you to select text on the terminal using keyboard shortcuts

   - [ligatures](https://st.suckless.org/patches/ligatures/)
      - adds support for drawing ligatures using the Harfbuzz library to transform original text of a single line to a list of glyphs with ligatures included

   - [monochrome](https://www.reddit.com/r/suckless/comments/ixbx6z/how_to_use_black_and_white_only_for_st/)
      - makes st ignore terminal color attributes to make for a monochrome look

   - [netwmicon](https://st.suckless.org/patches/netwmicon/)
      - sets the \_NET\_WM\_ICON X property with a .png file
      - or alternatively sets the \_NET\_WM\_ICON X property with a farbfeld (.ff) file
      - or alternatively sets the \_NET\_WM\_ICON X property with a hardcoded icon

   - [newterm](https://st.suckless.org/patches/newterm/)
      - allows you to spawn a new st terminal using Ctrl-Shift-Return
      - it will have the same CWD (current working directory) as the original st instance

   - [no-window-decorations](https://github.com/bakkeby/patches/wiki/no_window_decorations)
      - makes st show without window decorations if the WM supports it

   - [open-copied-url](https://st.suckless.org/patches/open_copied_url/)
      - open contents of the clipboard in a user-defined browser

   - [open-selected-text](https://st.suckless.org/patches/open_selected_text)
      - open the selected text using `xdg-open`

   - [openurlonclick](https://www.reddit.com/r/suckless/comments/cc83om/st_open_url/)
      - allows for URLs to be opened directly when you click on them

   - [~osc\_10\_11\_12\_2~](https://st.suckless.org/patches/osc_10_11_12_2/)
      - ~this patch adds support for OSC escape sequences 10, 11, and 12 in the way they are~
        ~implemented in most other terminals (e.g libvte, kitty)~
      - ~specifically it differs from~ [~osc_10_11_12~](https://st.suckless.org/patches/osc_10_11_12/)
        ~in that it treats the background and foreground colors as distinct from palette colours 01~
        ~and 07 in order to facilitate the use of theme setting scripts like~
        [~theme.sh~](https://github.com/lemnos/theme.sh) ~which expect these colours to be distinct~

   - reflow
      - allows st to be resized without cutting off text when the terminal window is made larger again
      - text wraps when the terminal window is made smaller

   - [relativeborder](https://st.suckless.org/patches/relativeborder/)
      - allows you to specify a border that is relative in size to the width of a cell in the
        terminal

   - [right-click-to-plumb](https://st.suckless.org/patches/right_click_to_plumb/)
      - allows you to right-click on some selected text to send it to the plumbing program of choice

   - [scrollback](https://st.suckless.org/patches/scrollback/)
      - allows you scroll back through terminal output using keyboard shortcuts or mousewheel

   - sixel
      - this patch adds SIXEL graphics support

   - st-embedder
      - this patch allows clients to embed into the st window and can be useful if you tend to
        start X applications from the terminal
      - the behavior is similar to Plan 9 where applications can take over windows

   - [spoiler](https://st.suckless.org/patches/spoiler/)
      - use inverted defaultbg/fg for selection when bg/fg are the same

   - [swapmouse](https://st.suckless.org/patches/swapmouse/)
      - changes the mouse shape to the global default when the running program subscribes for mouse
        events, for instance, in programs like ranger and fzf
      - it emulates the behaviour shown by vte terminals like termite

   - [sync](https://st.suckless.org/patches/sync/)
      - adds synchronized-updates/application-sync support in st
      - this has no effect except when an application uses the synchronized-update escape sequences
      - with this patch nearly all cursor flicker is eliminated in tmux, and tmux detects it
        automatically via terminfo

   - [themed-cursor](https://st.suckless.org/patches/themed_cursor/)
      - instead of a default X cursor, use the xterm cursor from your cursor theme

   - [undercurl](https://st.suckless.org/patches/undercurl/)
      - adds support for special underlines, e.g. curly / wavy underlines

   - [universcroll](https://st.suckless.org/patches/universcroll/)
      - allows mouse scroll without modifier keys for regardless of alt screen using the external
        scroll program

   - [use-XftFontMatch](https://git.suckless.org/st/commit/528241aa3835e2f1f052abeeaf891737712955a0.html)
      - use XftFontMatch in place of FcFontMatch to allow font to scale with Xft.dpi resource
        setting

   - [vertcenter](https://st.suckless.org/patches/vertcenter/)
      - vertically center lines in the space available if you have set a larger chscale in config.h

   - [~vim-browse~](https://st.suckless.org/patches/vim_browse/)
      - ~the vim-browse patch offers the possibility to move through the terminal history-buffer,~
        ~search for strings using VIM-like motions, operations and quantifiers~
      - ~it overlays the screen with highlighted search results and displays the current operation~
        ~/ motions / search string in the bottom right corner~
      - the VIM browse patch was removed due to sheer complexity and it being incompatible with a
        significant number of other patches
      - if you want to try this patch out then the recommendation is to play around with the
        author's own build of st where this is properly implemented with history buffer (scrollback)
      - https://github.com/juliusHuelsmann/st

   - [visualbell](https://st.suckless.org/patches/visualbell/)
      - adds visual indicators for the terminal bell event

   - [w3m](https://st.suckless.org/patches/w3m/)
      - adds support for w3m images

   - [wide-glyphs](https://www.reddit.com/r/suckless/comments/jt90ai/update_support_for_proper_glyph_rendering_in_st/)
      - adds proper support for wide glyphs, as opposed to rendering smaller or cut glyphs

   - [wide-glyph-spacing](https://github.com/googlefonts/Inconsolata/issues/42#issuecomment-737508890)
      - there is a known issue that Google's Variable Fonts (VF) can end up with letter spacing
        that is too wide in programs that use Xft, for example Inconsolata v3.000
      - this is intended as a temporary workaround / patch / hack until (if) this is fixed in the
        Xft library itself

   - [workingdir](https://st.suckless.org/patches/workingdir/)
      - allows user to specify the initial path st should use as the working directory

   - [xresources](https://st.suckless.org/patches/xresources/)
      - adds the ability to configure st via Xresources
      - during startup, st will read and apply the resources named in the resources[] array in config.h
