PLUGIN = aac-raw${PLUGIN_SUFFIX}

SRCS = aac.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${FAAD_CFLAGS} -I../..
LIBS += ${FAAD_LIBS} -lm -laudtag
