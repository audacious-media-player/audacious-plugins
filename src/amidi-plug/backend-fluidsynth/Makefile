PLUGIN = ap-fluidsynth${PLUGIN_SUFFIX}

SRCS = b-fluidsynth.c

include ../../../buildsys.mk
include ../../../extra.mk

plugindir := ${plugindir}/${AMIDIPLUG_BACKEND_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${FLUIDSYNTH_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${FLUIDSYNTH_CFLAGS} ${GLIB_CFLAGS} -I../../..
LIBS += ${FLUIDSYNTH_LIBS} ../pcfg/libpcfg.a
