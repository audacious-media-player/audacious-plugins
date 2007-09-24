include ../../extra.mk

PLUGIN = null${PLUGIN_SUFFIX}
plugindir = audacious/${OUTPUT_PLUGIN_DIR}

SRCS = null.c

include ../../buildsys.mk

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${DBUS_CFLAGS} ${MOWGLI_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}

