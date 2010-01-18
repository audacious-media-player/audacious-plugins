PLUGIN = compressor${PLUGIN_SUFFIX}

SRCS = compressor.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS} -I../..
LIBS += ${GTK_LIBS}
