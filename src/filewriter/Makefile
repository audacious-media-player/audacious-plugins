PLUGIN = filewriter${SHARED_SUFFIX}
SRCS = filewriter.c	\
       wav.c		\
       mp3.c		\
       vorbis.c		\
       flac.c

plugindir = audacious/${OUTPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${GTK_CFLAGS} ${PANGO_CFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${FILEWRITER_LIBS}
