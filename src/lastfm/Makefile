PLUGIN = lastfm${PLUGIN_SUFFIX}

SRCS = lastfm.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} ${CURL_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${XML_LIBS} ${CURL_LIBS} ${MOWGLI_LIBS}
