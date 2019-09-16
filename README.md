Similar to [dwm-flexipatch]() this project has a different take on st patching. It uses preprocessor directives to decide whether or not to include a patch during build time. Essentially this means that this build, for better or worse, contains both the patched _and_ the original code. The aim being that you can select which patches to include and the build will contain that code and nothing more.

For example to include the `alpha` patch then you would only need to flip this setting from 0 to 1 in [patches.h](https://github.com/bakkeby/dwm-flexipatch/blob/master/patches.h):
```c
#define ALPHA_PATCH 1
```

Refer to [https://dwm.suckless.org/](https://st.suckless.org/) for details on the st terminal, how to install it and how it works.

---

### Changelog:

2019-09-16 - Added alpha and anysize patches

### Patches included:

   - [alpha](https://st.suckless.org/patches/alpha/)
      - adds transparency for the terminal

   - [anysize](https://st.suckless.org/patches/anysize/)
      - allows st to reize to any pixel size rather than snapping to character width / height