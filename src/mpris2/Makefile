PLUGIN = mpris2${PLUGIN_SUFFIX}

SRCS = object-core.c object-player.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GLIB_CFLAGS} `pkg-config --cflags gio-unix-2.0`
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} `pkg-config --libs gio-unix-2.0`
