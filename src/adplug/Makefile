PLUGIN = adplug${PLUGIN_SUFFIX}

SRCS = adplug-xmms.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
# FIXME: Turning off warnings for now; this code is awful
CXXFLAGS += ${PLUGIN_CFLAGS} -Wno-sign-compare -Wno-shift-negative-value
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${ADLIB_CFLAGS} -I../..
LIBS += ${ADLIB_LIBS}
