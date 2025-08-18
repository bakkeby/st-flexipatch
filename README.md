# cst
> The c(ute)st suckless terminal build.

Adding a small set of patches to make `st` feel more like `alacritty`!  

## Patches
```c
#define CLIPBOARD_PATCH 1
#define COPYURL_HIGHLIGHT_SELECTED_PATCH 1
#define DISABLE_ITALIC_FONTS_PATCH 1
#define OPENCOPIED_PATCH 1
#define OPENURLONCLICK_PATCH 1
#define SCROLLBACK_PATCH 1
#define UNIVERSCROLL_PATCH 1
#define ST_EMBEDDER_PATCH 1
#define XRESOURCES_PATCH 1
```

The big ones here are the clipboard patch so you can use your system clipboard and the Xresources patch.

> [!NOTE]
> You can easily add or remove patches by editing `patches.def.h`!  

1. Get the dependencies!
```
xbps-install -Syu libX11-devel libXft-devel
```
2. Build it after making your changes
```
make
```
3. Install it when satisfied
```
make install
```

## Bindings
* ctrl + j/k to scroll
* mod1 + u to copy links
* mod1 + o to open links