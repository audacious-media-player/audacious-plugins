PLUGIN = null${PLUGIN_SUFFIX}

SRCS = null.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}   -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}
