PLUGIN = blur_scope-qt${PLUGIN_SUFFIX}

SRCS = blur_scope.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${QT_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${QT_LIBS} -lm -laudqt
