PLUGIN = cairo-spectrum${PLUGIN_SUFFIX}

SRCS = cairo-spectrum.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS}
LIBS += -lm ${GTK_LIBS}
