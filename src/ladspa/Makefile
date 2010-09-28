PLUGIN = ladspa${PLUGIN_SUFFIX}

SRCS = ladspa.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${GMODULE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${GMODULE_LIBS}
