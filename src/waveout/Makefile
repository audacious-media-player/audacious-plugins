PLUGIN = waveout${PLUGIN_SUFFIX}

SRCS = waveout.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../..
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lwinmm
