PLUGIN = gntui${PLUGIN_SUFFIX}

SRCS = gntui.c \
       fileselector.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

GNT_CFLAGS = `pkg-config --cflags gnt`
GNT_LIBS = `pkg-config --libs gnt`

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${GNT_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${GNT_LIBS}
