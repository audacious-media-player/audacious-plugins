PLUGIN = flacng${PLUGIN_SUFFIX}

SRCS = plugin.cc \
       tools.cc \
       seekable_stream_callbacks.cc	\
       metadata.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${LIBFLAC_CFLAGS} -I../..
LIBS += ${LIBFLAC_LIBS}
