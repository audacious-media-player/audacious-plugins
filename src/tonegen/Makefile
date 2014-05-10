PLUGIN = tonegen${PLUGIN_SUFFIX}

SRCS = tonegen.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += ${GLIB_LIBS} -lm
