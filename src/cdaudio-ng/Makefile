PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}

SRCS = cdaudio-ng.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${CDIO_CFLAGS} ${CDDB_CFLAGS} -I../..
LIBS += ${GLIB_LIBS}  ${CDIO_LIBS} ${CDDB_LIBS}
