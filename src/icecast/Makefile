PLUGIN = icecast${PLUGIN_SUFFIX}

SRCS = icecast.c		\
       ../filewriter/mp3.o	\
       ../filewriter/vorbis.o	\
       ../filewriter/convert.o

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${SHOUT_CFLAGS}
CPPFLAGS += ${GTK_CFLAGS} ${DBUS_CFLAGS} ${FILEWRITER_CFLAGS} ${OGG_VORBIS_CFLAGS} ${SHOUT_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${FILEWRITER_LIBS} ${SHOUT_LIBS}
