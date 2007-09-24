PLUGIN = OSS4${PLUGIN_SUFFIX}
SRCS = OSS4.c		\
       about.c		\
       configure.c	\
       audio.c		\
       init.c		\
       convert.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} -I/usr/lib/oss/include -I../../intl -I../.. ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${OSS_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
