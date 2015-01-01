PLUGIN = jack-ng${PLUGIN_SUFFIX}

SRCS = jack-ng.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${JACK_CFLAGS} -I../..
LIBS += ${JACK_LIBS}
