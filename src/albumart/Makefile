PLUGIN = albumart${PLUGIN_SUFFIX}

SRCS = albumart.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS}
LIBS += ${GTK_LIBS}
