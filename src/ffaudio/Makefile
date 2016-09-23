PLUGIN = ffaudio${PLUGIN_SUFFIX}

SRCS = ffaudio-core.cc ffaudio-io.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${FFMPEG_CFLAGS} -I../..
LIBS += ${FFMPEG_LIBS} -laudtag
