PLUGIN = expander${PLUGIN_SUFFIX}

SRCS = expander.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} -I../..
LIBS += ${GTK_LIBS} -lm
