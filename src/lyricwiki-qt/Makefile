PLUGIN = lyricwiki-qt${PLUGIN_SUFFIX}

SRCS = lyricwiki.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${QT_CFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${QT_LIBS} ${GLIB_LIBS}  ${XML_LIBS}
