PLUGIN = bs2b${PLUGIN_SUFFIX}

SRCS = plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${BS2B_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${MOWGLI_LIBS} ${BS2B_LIBS}
