PLUGIN = opus${PLUGIN_SUFFIX}

SRCS = opus.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${OPUS_CFLAGS} -I../..
LIBS += ${OPUS_LIBS}
