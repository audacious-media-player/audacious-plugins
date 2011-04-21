PLUGIN = inet${PLUGIN_SUFFIX}

SRCS = inet.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lwininet
