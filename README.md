Similar to [dwm-flexipatch]() this project has a different take on st patching. It uses preprocessor directives to decide whether or not to include a patch during build time. Essentially this means that this build, for better or worse, contains both the patched _and_ the original code. The aim being that you can select which patches to include and the build will contain that code and nothing more.

For example to include the `alpha` patch then you would only need to flip this setting from 0 to 1 in [patches.h](https://github.com/bakkeby/dwm-flexipatch/blob/master/patches.h):
```c
#define ALPHA_PATCH 1
```

Refer to [https://dwm.suckless.org/](https://st.suckless.org/) for details on the st terminal, how to install it and how it works.

---

### Changelog:

2019-09-16 - Added alpha, anysize, bold-is-not-bright, clipboard, copyurl, disable-fonts, fixime, hidecursor, newterm, open-copied-url, vertcenter, scrollback and xresources patches

### Patches included:

   - [alpha](https://st.suckless.org/patches/alpha/)
      - adds transparency for the terminal

   - [anysize](https://st.suckless.org/patches/anysize/)
      - allows st to reize to any pixel size rather than snapping to character width / height

   - [bold-is-not-bright](https://st.suckless.org/patches/bold-is-not-bright/)
      - by default bold text is rendered with a bold font in the bright variant of the current color
      - this patch makes bold text rendered simply as bold, leaving the color unaffected

   - [clipboard](https://st.suckless.org/patches/clipboard/)
      - by default st only sets PRIMARY on selection
      - this patch makes st set CLIPBOARD on selection

   - [copyurl](https://st.suckless.org/patches/copyurl/)
      - this patch allows you to select and copy the last URL displayed with Mod+l
      - multiple invocations cycle through the available URLs

   - [disable-fonts](https://st.suckless.org/patches/disable_bold_italic_fonts/)
      - this patch adds the option of disabling bold/italic/roman fonts globally

   - [fixime](https://st.suckless.org/patches/fix_ime/)
      - adds better Input Method Editor (IME) support

   - [hidecursor](https://st.suckless.org/patches/hidecursor/)
      - hides the X cursor whenever a key is pressed and show it back when the mouse is moved in the terminal window

   - [newterm](https://st.suckless.org/patches/newterm/)
      - allows you to spawn a new st terminal using Ctrl-Shift-Return
      - it will have the same CWD (current working directory) as the original st instance

   - [open-copied-url](https://st.suckless.org/patches/open_copied_url/)
      - open contents of the clipboard in a user-defined browser

   - [scrollback](https://st.suckless.org/patches/scrollback/)
      - allows you scroll back through terminal output using keyboard shortcuts or mousewheel

   - [vertcenter](https://st.suckless.org/patches/vertcenter/)
      - vertically center lines in the space available if you have set a larger chscale in config.h

   - [xresources](https://st.suckless.org/patches/xresources/)
      - adds the ability to configure st via Xresources
      - during startup, st will read and apply the resources named in the resources[] array in config.h
