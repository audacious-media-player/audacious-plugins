PLUGIN = hotkey${PLUGIN_SUFFIX}

SRCS = plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${MOWGLI_CFLAGS} ${GLIB_CFLAGS} ${DBUS_CFLAGS} -I../../intl -I../.. -I..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} ${DBUS_LIBS}
