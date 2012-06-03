PLUGIN = pulse_audio${PLUGIN_SUFFIX}

SRCS = pulse_audio.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -lpulse
