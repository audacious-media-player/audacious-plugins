PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}
SRCS = cdaudio-ng.c	\
       configure.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS = ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${CDIO_CFLAGS} ${CDDB_CFLAGS} -I../.. ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CDIO_LIBS} ${CDDB_LIBS}
