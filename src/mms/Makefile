PLUGIN = mms${PLUGIN_SUFFIX}
SRCS = mms.c

plugindir = audacious/${TRANSPORT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} ${MMS_CFLAGS} -I../../intl -I../.. -Wall
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${XML_LIBS} ${MMS_LIBS}
