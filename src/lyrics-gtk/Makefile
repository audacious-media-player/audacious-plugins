PLUGIN = lyrics-gtk${PLUGIN_SUFFIX}

SRCS = ../lyrics-common/chart_lyrics_provider.cc \
       ../lyrics-common/file_provider.cc \
       ../lyrics-common/lrclib_provider.cc \
       ../lyrics-common/lyrics_ovh_provider.cc \
       ../lyrics-common/utils.cc \
       lyrics-gtk.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${JSON_GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${JSON_GLIB_LIBS} ${XML_LIBS} -laudgui
