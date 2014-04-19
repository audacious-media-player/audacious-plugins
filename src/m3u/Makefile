PLUGIN = m3u${PLUGIN_SUFFIX}

SRCS = m3u.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += ${GLIB_LIBS}
