PLUGIN = pipewire${PLUGIN_SUFFIX}

SRCS = pipewire.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${PIPEWIRE_CFLAGS} -I../..
LIBS += ${PIPEWIRE_LIBS}
