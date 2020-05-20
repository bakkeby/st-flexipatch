Similar to [dwm-flexipatch](https://github.com/bakkeby/dwm-flexipatch) this st 0.8.3 (d66111, 2020-05-09) project has a different take on st patching. It uses preprocessor directives to decide whether or not to include a patch during build time. Essentially this means that this build, for better or worse, contains both the patched _and_ the original code. The aim being that you can select which patches to include and the build will contain that code and nothing more.

For example to include the `alpha` patch then you would only need to flip this setting from 0 to 1 in [patches.h](https://github.com/bakkeby/st-flexipatch/blob/master/patches.def.h):
```c
#define ALPHA_PATCH 1
```

Once you have found out what works for you and what doesn't then you should be in a better position to choose patches should you want to start patching from scratch.

Alternatively if you have found the patches you want, but don't want the rest of the flexipatch entanglement on your plate then you may want to have a look at [flexipatch-finalizer](https://github.com/bakkeby/flexipatch-finalizer); a custom pre-processor tool that removes all the unused flexipatch code leaving you with a build that contains the patches you selected.

Refer to [https://st.suckless.org/](https://st.suckless.org/) for details on the st terminal, how to install it and how it works.

---

### Changelog:

2020-05-20 - Upgrade to d66111, 2020-05-09, removed visualbell 1, 2, 3 patches and force redraw after keypress due to incompatibility

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

   - [anysize](https://st.suckless.org/patches/anysize/)
      - allows st to reize to any pixel size rather than snapping to character width / height

   - [bold-is-not-bright](https://st.suckless.org/patches/bold-is-not-bright/)
      - by default bold text is rendered with a bold font in the bright variant of the current color
      - this patch makes bold text rendered simply as bold, leaving the color unaffected

   - [boxdraw](https://st.suckless.org/patches/boxdraw/)
      - adds dustom rendering of lines/blocks/braille characters for gapless alignment

   - [clipboard](https://st.suckless.org/patches/clipboard/)
      - by default st only sets PRIMARY on selection
      - this patch makes st set CLIPBOARD on selection

   - [copyurl](https://st.suckless.org/patches/copyurl/)
      - this patch allows you to select and copy the last URL displayed with Mod+l
      - multiple invocations cycle through the available URLs

   - [disable-fonts](https://st.suckless.org/patches/disable_bold_italic_fonts/)
      - this patch adds the option of disabling bold/italic/roman fonts globally

   - [externalpipe](https://st.suckless.org/patches/externalpipe/)
      - this patch allows for eading and writing st's screen through a pipe, e.g. to pass info to dmenu

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

   - [hidecursor](https://st.suckless.org/patches/hidecursor/)
      - hides the X cursor whenever a key is pressed and show it back when the mouse is moved in the terminal window

   - [invert](https://st.suckless.org/patches/invert/)
      - adds a keybinding that lets you invert the current colorscheme of st
      - this provides a simple way to temporarily switch to a light colorscheme if you use a dark colorscheme or visa-versa

   - [iso14755](https://st.suckless.org/patches/iso14755/)
      - pressing the default binding Ctrl+Shift-i will popup dmenu, asking you to enter a unicode codepoint that will be converted to a glyph and then pushed to st

   - [keyboard-select](https://st.suckless.org/patches/keyboard_select/)
      - allows you to select text on the terminal using keyboard shortcuts

   - [newterm](https://st.suckless.org/patches/newterm/)
      - allows you to spawn a new st terminal using Ctrl-Shift-Return
      - it will have the same CWD (current working directory) as the original st instance

   - [open-copied-url](https://st.suckless.org/patches/open_copied_url/)
      - open contents of the clipboard in a user-defined browser

   - [relativeborder](https://st.suckless.org/patches/relativeborder/)
      - allows you to specify a border that is relative in size to the width of a cell in the terminal

   - [right-click-to-plumb](https://st.suckless.org/patches/right_click_to_plumb/)
      - allows you to right-click on some selected text to send it to the plumbing program of choice

   - [scrollback](https://st.suckless.org/patches/scrollback/)
      - allows you scroll back through terminal output using keyboard shortcuts or mousewheel

   - st-embedder
      - this patch allows clients to embed into the st window and can be useful if you tend to start X applications from the terminal
      - the behavior is similar to Plan 9 where applications can take over windows

   - [spoiler](https://st.suckless.org/patches/spoiler/)
      - use inverted defaultbg/fg for selection when bg/fg are the same

   - [themed-cursor](https://st.suckless.org/patches/themed_cursor/)
      - instead of a default X cursor, use the xterm cursor from your cursor theme

   - [vertcenter](https://st.suckless.org/patches/vertcenter/)
      - vertically center lines in the space available if you have set a larger chscale in config.h

   - [~visualbell~](https://st.suckless.org/patches/visualbell/)
      - ~adds visual indicators for the terminal bell event~

   - [workingdir](https://st.suckless.org/patches/workingdir/)
      - allows user to specify the initial path st should use as the working directory

   - [xresources](https://st.suckless.org/patches/xresources/)
      - adds the ability to configure st via Xresources
      - during startup, st will read and apply the resources named in the resources[] array in config.h
