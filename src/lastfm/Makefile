PLUGIN = lastfm${PLUGIN_SUFFIX}

SRCS = lastfm.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} ${CURL_CFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${XML_LIBS} ${CURL_LIBS}
