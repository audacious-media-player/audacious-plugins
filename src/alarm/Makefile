PLUGIN = alarm${PLUGIN_SUFFIX}

SRCS = alarm.cc		\
       interface.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} -I../..
LIBS += ${GTK_LIBS} -laudgui
