PLUGIN = search-tool${PLUGIN_SUFFIX}

SRCS = search-tool.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -std=gnu99 -I../..
CFLAGS += ${PLUGIN_CFLAGS} ${GTK_CFLAGS}
LIBS += ${GTK_LIBS}
