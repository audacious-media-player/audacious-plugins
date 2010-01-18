PLUGIN = ap-fluidsynth${PLUGIN_SUFFIX}

SRCS = b-fluidsynth.c

include ../../../buildsys.mk
include ../../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}/${AMIDIPLUG_BACKEND_DIR}

CFLAGS += ${FLUIDSYNTH_CFLAGS}
CPPFLAGS += ${FLUIDSYNTH_CFLAGS} -I../../..
LIBS += ${FLUIDSYNTH_LIBS} ../pcfg/libpcfg.a
