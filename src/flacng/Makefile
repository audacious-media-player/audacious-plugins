PLUGIN = flacng${PLUGIN_SUFFIX}

SRCS = plugin.c \
       tools.c \
       seekable_stream_callbacks.c	\
       metadata.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${LIBFLAC_CFLAGS} -I../..
LIBS += ${LIBFLAC_LIBS}
