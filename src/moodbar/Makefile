PLUGIN = moodbar${PLUGIN_SUFFIX}

SRCS = moodbar.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${XML_LIBS}
