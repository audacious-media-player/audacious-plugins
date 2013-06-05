PLUGIN = asx${PLUGIN_SUFFIX}
SRCS = asx.c util.c asxv3.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${XML_LIBS}
