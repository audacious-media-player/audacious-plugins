PLUGIN = tta${PLUGIN_SUFFIX}
SRCS = libtta.c	\
       ttadec.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -L${libdir} -laudid3tag ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
