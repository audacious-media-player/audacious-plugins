PLUGIN = song-info-qt${PLUGIN_SUFFIX}

SRCS = song-info.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += ${QT_LIBS} -laudqt
