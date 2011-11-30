PLUGIN = gnomeshortcuts${PLUGIN_SUFFIX}

SRCS = gnomeshortcuts.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${DBUS_CFLAGS} -I../.. -I..
LIBS += ${GLIB_LIBS} ${DBUS_LIBS} ${GTK_LIBS}
