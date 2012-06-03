PLUGIN = compressor${PLUGIN_SUFFIX}

SRCS = compressor.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -lm
