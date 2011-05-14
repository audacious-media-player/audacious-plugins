PLUGIN = cd-menu-items${PLUGIN_SUFFIX}

SRCS = cd-menu-items.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -std=gnu99 -I../..
CFLAGS += ${PLUGIN_CFLAGS} ${GTK_CFLAGS}
LIBS += ${GTK_LIBS}
