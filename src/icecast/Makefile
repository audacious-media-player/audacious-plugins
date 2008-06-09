PLUGIN = icecast${PLUGIN_SUFFIX}

SRCS = icecast.c	\
       ../filewriter/mp3.o		\
       ../filewriter/vorbis.o

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${GTK_CFLAGS} ${PANGO_CFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${FILEWRITER_CFLAGS} ${OGG_VORBIS_CFLAGS} -I../../intl -I../..
LIBS += ${GTK_LIBS} ${FILEWRITER_LIBS}
