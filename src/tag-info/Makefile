PLUGIN = tag-info${PLUGIN_SUFFIX}
SRCS = tag-info.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} -laudgui
