PLUGIN = null${PLUGIN_SUFFIX}
SRCS = null.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${DBUS_CFLAGS} ${MOWGLI_CFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
