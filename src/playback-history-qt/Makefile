PLUGIN = playback-history-qt${PLUGIN_SUFFIX}

SRCS = playback-history.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. ${QT_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${QT_LIBS} -laudqt
