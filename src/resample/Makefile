PLUGIN = resample${PLUGIN_SUFFIX}

SRCS = resample.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} -I../..
LIBS += ${GTK_LIBS} -lsamplerate
