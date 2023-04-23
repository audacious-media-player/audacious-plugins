PLUGIN = adplug${PLUGIN_SUFFIX}

SRCS = adplug-xmms.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${ADLIB_CFLAGS} -I../..
LIBS += ${ADLIB_LIBS}
