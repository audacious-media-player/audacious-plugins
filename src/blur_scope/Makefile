PLUGIN = blur_scope${PLUGIN_SUFFIX}

SRCS = blur_scope.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} -I../..
LIBS += ${GTK_LIBS} -lm
