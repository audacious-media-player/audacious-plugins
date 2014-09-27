PLUGIN = gnomeshortcuts${PLUGIN_SUFFIX}

SRCS = gnomeshortcuts.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${DBUS_CFLAGS} -I../.. -I..
LIBS += ${GLIB_LIBS} ${DBUS_LIBS} ${GTK_LIBS}
