PLUGIN = demac${PLUGIN_SUFFIX}

SRCS = plugin.c	\
       apev2.c \
       ape.c \
       apedec.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} -I../../intl -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${MOWGLI_LIBS}
