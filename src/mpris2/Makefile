PLUGIN = mpris2${PLUGIN_SUFFIX}

SRCS = object-core.c object-player.c plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GLIB_CFLAGS} ${GIO_UNIX_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} ${GIO_UNIX_LIBS}
