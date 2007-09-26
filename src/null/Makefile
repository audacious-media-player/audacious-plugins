PLUGIN = null${PLUGIN_SUFFIX}
SRCS = null.c

plugindir = audacious/${OUTPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${DBUS_CFLAGS} ${MOWGLI_CFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
