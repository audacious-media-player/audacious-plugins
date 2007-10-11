PLUGIN = flacng${PLUGIN_SUFFIX}

SRCS = plugin.c				\
       tools.c				\
       seekable_stream_callbacks.c	\
       flac_compat112.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${LIBFLAC_CFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
LIBS += ${LIBFLAC_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
