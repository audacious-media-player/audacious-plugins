PLUGIN = vtx${SHARED_SUFFIX}
SRCS = about.c		\
       ay8912.c		\
       config.c		\
       info.c		\
       lh5dec.c		\
       vtx.c		\
       vtxfile.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} -I../../intl -I../.. -I.
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
