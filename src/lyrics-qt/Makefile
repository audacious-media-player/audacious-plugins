PLUGIN = lyrics-qt${PLUGIN_SUFFIX}

SRCS = ../lyrics-common/chart_lyrics_provider.cc \
       ../lyrics-common/file_provider.cc \
       ../lyrics-common/lrclib_provider.cc \
       ../lyrics-common/lyrics_ovh_provider.cc \
       ../lyrics-common/utils.cc \
       lyrics-qt.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${QT_CFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${QT_LIBS} ${GLIB_LIBS} ${XML_LIBS}
