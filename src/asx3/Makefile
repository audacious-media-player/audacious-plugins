PLUGIN = asx3${PLUGIN_SUFFIX}

SRCS = asx3.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${XML_LIBS}
