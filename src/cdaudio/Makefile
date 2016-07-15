PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}

SRCS = cdaudio-ng.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${CDIO_CFLAGS} -I../..
LIBS += ${CDIO_LIBS}
