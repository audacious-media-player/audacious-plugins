PLUGIN = streamtuner${PLUGIN_SUFFIX}

SRCS = \
	streamtuner.cc \
	shoutcast-widget.cc \
	shoutcast-model.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${QT_CFLAGS} ${QTNETWORK_CFLAGS} -I../..
LIBS += ${QT_LIBS} ${QTNETWORK_LIBS} -laudqt
