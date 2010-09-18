PLUGIN = flacng${PLUGIN_SUFFIX}

SRCS = plugin.c \
       tools.c \
       seekable_stream_callbacks.c	\
       flac_compat112.c \
       metadata.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${LIBFLAC_CFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += ${LIBFLAC_LIBS} ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS}
