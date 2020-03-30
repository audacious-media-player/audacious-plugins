PLUGIN = qt-spectrum${PLUGIN_SUFFIX}

SRCS = qt-spectrum.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += -lm ${QT_LIBS} -laudqt
