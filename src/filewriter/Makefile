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

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${FILEWRITER_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${FILEWRITER_LIBS}
