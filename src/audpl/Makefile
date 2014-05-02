PLUGIN = audpl${PLUGIN_SUFFIX}

SRCS = audpl.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../..
CFLAGS += ${PLUGIN_CFLAGS}
