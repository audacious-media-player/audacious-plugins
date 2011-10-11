PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}

SRCS = cdaudio-ng.c	\
       configure.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += -std=gnu99 ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${CDIO_CFLAGS} ${CDDB_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${CDIO_LIBS} ${CDDB_LIBS} ${MOWGLI_LIBS}
