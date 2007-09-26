PLUGIN = gnomeshortcuts${PLUGIN_SUFFIX}
SRCS = gnomeshortcuts.c

plugindir = audacious/${GENERAL_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${MOWGLI_CFLAGS} ${GLIB_CFLAGS} ${DBUS_CFLAGS} -I../../intl -I../.. -I..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} ${DBUS_LIBS}
