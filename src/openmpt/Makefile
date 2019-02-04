PLUGIN = openmpt${PLUGIN_SUFFIX}

SRCS = mpt.cc \
       mptwrap.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${OPENMPT_CFLAGS} -I../..
LIBS += ${OPENMPT_LIBS}
