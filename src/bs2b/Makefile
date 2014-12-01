PLUGIN = bs2b${PLUGIN_SUFFIX}

SRCS = plugin.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BS2B_CFLAGS} -I../..
LIBS += ${BS2B_LIBS}
