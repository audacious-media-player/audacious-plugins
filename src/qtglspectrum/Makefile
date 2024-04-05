PLUGIN = gl-spectrum-qt${PLUGIN_SUFFIX}

SRCS = gl-spectrum.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS} ${QTOPENGL_CFLAGS} ${GL_CFLAGS}
LIBS += -lm ${QT_LIBS} ${QTOPENGL_LIBS} ${GL_LIBS}
