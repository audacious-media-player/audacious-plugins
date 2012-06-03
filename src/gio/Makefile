PLUGIN = gio${PLUGIN_SUFFIX}

SRCS = gio.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GIO_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GIO_LIBS}
