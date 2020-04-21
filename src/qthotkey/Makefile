PLUGIN = qthotkey${PLUGIN_SUFFIX}

SRCS = plugin.cc gui.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${QT_CFLAGS} ${QTX11EXTRAS_CFLAGS}  -I../.. -I..
LIBS += ${QT_LIBS} ${QTX11EXTRAS_LIBS} -lX11 -laudqt
