PLUGIN = lyricwiki${PLUGIN_SUFFIX}

SRCS = lyricwiki.c urlencode.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${XML_LIBS}
