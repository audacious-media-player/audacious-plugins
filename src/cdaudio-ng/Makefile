PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}

SRCS = cdaudio-ng.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${CDIO_CFLAGS} ${CDDB_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${CDIO_LIBS} ${CDDB_LIBS}
