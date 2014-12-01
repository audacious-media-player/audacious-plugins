PLUGIN = mixer${PLUGIN_SUFFIX}

SRCS = mixer.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
