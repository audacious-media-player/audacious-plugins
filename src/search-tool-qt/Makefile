PLUGIN = search-tool-qt${PLUGIN_SUFFIX}

SRCS = search-tool-qt.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. ${GLIB_CFLAGS} ${QT_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} ${QT_LIBS} -laudqt
