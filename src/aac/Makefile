PLUGIN = aac-raw${PLUGIN_SUFFIX}

SRCS = aac.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -lfaad -lm -laudtag
