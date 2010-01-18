PLUGIN = flacng${PLUGIN_SUFFIX}

SRCS = plugin.c				\
       tools.c				\
       seekable_stream_callbacks.c	\
       flac_compat112.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${LIBFLAC_CFLAGS}  ${GTK_CFLAGS}  -I../..
LIBS += ${LIBFLAC_LIBS} ${GTK_LIBS}
