PLUGIN = asx${PLUGIN_SUFFIX}
SRCS = asx.c util.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}
