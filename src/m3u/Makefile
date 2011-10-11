PLUGIN = m3u${PLUGIN_SUFFIX}

SRCS = m3u.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += -std=gnu99 ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} -I../.. -Wall
LIBS += ${GTK_LIBS} ${GLIB_LIBS}
