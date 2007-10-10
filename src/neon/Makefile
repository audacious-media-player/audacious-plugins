PLUGIN = neon${PLUGIN_SUFFIX}

SRCS = neon.c	\
       rb.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} ${NEON_CFLAGS} -I../../intl -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${XML_LIBS} ${NEON_LIBS}
