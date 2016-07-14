PLUGIN = speed-pitch${PLUGIN_SUFFIX}

SRCS = speed-pitch.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm -lsamplerate
