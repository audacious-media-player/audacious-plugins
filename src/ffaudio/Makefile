PLUGIN = ffaudio${PLUGIN_SUFFIX}

SRCS = ffaudio-core.c ffaudio-io.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} $${FFMPEG_CFLAGS} -I../.. -std=c99 ${GCC42_CFLAGS} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${FFMPEG_LIBS} -laudtag
