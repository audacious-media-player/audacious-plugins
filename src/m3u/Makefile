PLUGIN = m3u${PLUGIN_SUFFIX}
SRCS = m3u.c

plugindir = audacious/${CONTAINER_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../../intl -I../.. -Wall
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${XML_LIBS}
