PLUGIN = madplug${PLUGIN_SUFFIX}
SRCS = configure.c	\
       dither.c		\
       input.c		\
       replaygain.c	\
       decoder.c	\
       fileinfo.c	\
       plugin.c		\
       xing.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${MOWGLI_CFLAGS} ${ARCH_DEFINES} ${SIMD_CFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -L${libdir} -laudid3tag -lmad ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
