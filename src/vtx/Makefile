PLUGIN = vtx${PLUGIN_SUFFIX}

SRCS = about.c		\
       ay8912.c		\
       info.c		\
       lh5dec.c		\
       vtx.c		\
       vtxfile.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../.. -I.
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS}
