PLUGIN = cue${PLUGIN_SUFFIX}

SRCS = cue.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS += -std=gnu99 ${PLUGIN_CPPFLAGS} ${CUE_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${CUE_LIBS}
