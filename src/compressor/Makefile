PLUGIN = compressor${PLUGIN_SUFFIX}

SRCS = compressor.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += -lm ${GLIB_LIBS}
