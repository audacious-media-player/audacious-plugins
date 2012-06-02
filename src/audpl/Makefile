PLUGIN = audpl${PLUGIN_SUFFIX}

SRCS = audpl.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS += -I../..
CFLAGS += ${PLUGIN_CFLAGS}
