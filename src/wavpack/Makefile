PLUGIN = wavpack${PLUGIN_SUFFIX}

SRCS = wavpack.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${WAVPACK_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${WAVPACK_LIBS} -laudtag
