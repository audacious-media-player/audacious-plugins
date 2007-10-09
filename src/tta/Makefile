PLUGIN = tta${PLUGIN_SUFFIX}
SRCS = libtta.c	\
       ttadec.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -L${libdir} -laudid3tag ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
