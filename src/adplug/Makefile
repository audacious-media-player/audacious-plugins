PLUGIN = adplug${PLUGIN_SUFFIX}

SRCS = adplug-xmms.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${QT_CFLAGS} ${ADLIB_CFLAGS} -I../..
ifeq ($(USE_QT),yes)
	LIBS += ${QT_LIBS} ${ADLIB_LIBS} -laudqt
else
	LIBS += ${GTK_LIBS} ${ADLIB_LIBS}
endif