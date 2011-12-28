PLUGIN = search-tool${PLUGIN_SUFFIX}

SRCS = search-tool.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS}
