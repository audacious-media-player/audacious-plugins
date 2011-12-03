PLUGIN = bs2b${PLUGIN_SUFFIX}

SRCS = plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${BS2B_CFLAGS}-I../..
LIBS += ${GTK_LIBS} ${BS2B_LIBS}
