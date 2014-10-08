PLUGIN = gl-spectrum-qt${PLUGIN_SUFFIX}

SRCS = gl-spectrum.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QTOPENGL_CFLAGS} ${GL_CFLAGS}
LIBS += -lm ${QTOPENGL_LIBS} ${GL_LIBS}
