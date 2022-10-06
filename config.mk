# st version
VERSION = 0.9

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

PKG_CONFIG = pkg-config

# Uncomment this for the alpha patch / ALPHA_PATCH
#XRENDER = -lXrender

# Uncomment this for the themed cursor patch / THEMED_CURSOR_PATCH
#XCURSOR = -lXcursor

# Uncomment the lines below for the ligatures patch / LIGATURES_PATCH
#LIGATURES_C = hb.c
#LIGATURES_H = hb.h
#LIGATURES_INC = `$(PKG_CONFIG) --cflags harfbuzz`
#LIGATURES_LIBS = `$(PKG_CONFIG) --libs harfbuzz`

# Uncomment this for the SIXEL patch / SIXEL_PATCH
#SIXEL_C = sixel.c sixel_hls.c

# includes and libs, uncomment harfbuzz for the ligatures patch
INCS = -I$(X11INC) \
       `$(PKG_CONFIG) --cflags fontconfig` \
       `$(PKG_CONFIG) --cflags freetype2` \
       $(LIGATURES_INC)
LIBS = -L$(X11LIB) -lm -lrt -lX11 -lutil -lXft ${XRENDER} ${XCURSOR}\
       `$(PKG_CONFIG) --libs fontconfig` \
       `$(PKG_CONFIG) --libs freetype2` \
       $(LIGATURES_LIBS)

# flags
STCPPFLAGS = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=600
STCFLAGS = $(INCS) $(STCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
STLDFLAGS = $(LIBS) $(LDFLAGS)

# OpenBSD:
#CPPFLAGS = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
#LIBS = -L$(X11LIB) -lm -lX11 -lutil -lXft \
#       `pkg-config --libs fontconfig` \
#       `pkg-config --libs freetype2`
#MANPREFIX = ${PREFIX}/man

# compiler and linker
# CC = c99
