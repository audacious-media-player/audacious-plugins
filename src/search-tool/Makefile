PLUGIN = search-tool${PLUGIN_SUFFIX}

SRCS = search-tool.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} -laudgui
