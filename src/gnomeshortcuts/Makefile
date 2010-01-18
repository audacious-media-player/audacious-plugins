PLUGIN = gnomeshortcuts${PLUGIN_SUFFIX}

SRCS = gnomeshortcuts.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS} ${DBUS_CFLAGS} -I../.. -I..
LIBS += ${DBUS_LIBS} ${GTK_LIBS}
