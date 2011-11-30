PLUGIN = filewriter${PLUGIN_SUFFIX}

SRCS = filewriter.c	\
       wav.c		\
       mp3.c		\
       vorbis.c		\
       flac.c           \
       convert.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${GTK_CFLAGS}   ${FILEWRITER_CFLAGS} ${OGG_VORBIS_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${FILEWRITER_LIBS}
