PLUGIN = madplug${PLUGIN_SUFFIX}
SRCS = configure.c	\
       dither.c		\
       input.c		\
       replaygain.c	\
       decoder.c	\
       fileinfo.c	\
       plugin.c		\
       xing.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${MOWGLI_CFLAGS} ${ARCH_DEFINES} -I../..
CFLAGS += ${PLUGIN_CFLAGS} ${SIMD_CFLAGS}
LIBS += -L${libdir} -laudid3tag -lmad ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
