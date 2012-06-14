PLUGIN = speed-pitch${PLUGIN_SUFFIX}

SRCS = speed-pitch.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm -lsamplerate
