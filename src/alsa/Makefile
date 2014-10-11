PLUGIN = alsa${PLUGIN_SUFFIX}

SRCS = alsa.cc \
       config.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${ALSA_CFLAGS} -I../..
LIBS += ${ALSA_LIBS}
