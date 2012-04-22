PLUGIN = gio${PLUGIN_SUFFIX}

SRCS = gio.c gtk.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GIO_CFLAGS} ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GIO_LIBS} ${GTK_LIBS}
