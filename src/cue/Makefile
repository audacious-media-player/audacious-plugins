PLUGIN = cue${PLUGIN_SUFFIX}

SRCS = cue.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${PLUGIN_CPPFLAGS} ${CUE_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${CUE_LIBS}
