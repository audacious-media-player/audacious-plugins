PLUGIN = pulse_audio${PLUGIN_SUFFIX}

SRCS = pulse_audio.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -lpulse
