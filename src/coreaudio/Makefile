PLUGIN = coreaudio${PLUGIN_SUFFIX}

SRCS = coreaudio.cc audiodevice.mm

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../..
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm -framework AudioUnit -framework CoreAudio -framework Foundation
