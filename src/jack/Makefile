PLUGIN = jackout${PLUGIN_SUFFIX}

SRCS = jack.cc		\
       bio2jack.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += ${JACK_LIBS} -lm
