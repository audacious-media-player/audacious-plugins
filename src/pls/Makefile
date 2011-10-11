PLUGIN = pls${PLUGIN_SUFFIX}
SRCS = pls.c util.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}
