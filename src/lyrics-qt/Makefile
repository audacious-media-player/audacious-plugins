PLUGIN = lyrics-qt${PLUGIN_SUFFIX}

SRCS = ../lyrics-common/file_provider.cc \
       ../lyrics-common/lrclib_provider.cc \
       ../lyrics-common/lyrics_ovh_provider.cc \
       ../lyrics-common/utils.cc \
       lyrics-qt.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${QT_CFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += ${QT_LIBS} ${GLIB_LIBS}
