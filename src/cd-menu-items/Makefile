PLUGIN = cd-menu-items${PLUGIN_SUFFIX}

SRCS = cd-menu-items.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS}
