PLUGIN = filewriter${PLUGIN_SUFFIX}

SRCS = filewriter.cc	\
       wav.cc		\
       mp3.cc		\
       vorbis.cc		\
       flac.cc           \
       convert.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${GTK_CFLAGS} ${FILEWRITER_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${FILEWRITER_LIBS}
