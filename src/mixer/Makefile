PLUGIN = mixer${PLUGIN_SUFFIX}

SRCS = mixer.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS}
