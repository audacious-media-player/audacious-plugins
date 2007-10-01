PLUGIN = hotkey${PLUGIN_SUFFIX}
plugindir = audacious/${GENERAL_PLUGIN_DIR}

SRCS = plugin.c

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${MOWGLI_CFLAGS} ${GLIB_CFLAGS} ${DBUS_CFLAGS} -I../../intl -I../.. -I..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} ${DBUS_LIBS}
