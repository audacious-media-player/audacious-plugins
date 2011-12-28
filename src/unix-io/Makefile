PLUGIN = unix-io${PLUGIN_SUFFIX}

SRCS = gtk.c unix-io.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS}