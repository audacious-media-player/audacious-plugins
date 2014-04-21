PLUGIN = flacng${PLUGIN_SUFFIX}

SRCS = plugin.cc \
       tools.cc \
       seekable_stream_callbacks.cc	\
       metadata.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${LIBFLAC_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${LIBFLAC_LIBS}
