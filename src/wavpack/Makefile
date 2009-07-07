PLUGIN = wavpack${PLUGIN_SUFFIX}

SRCS = tags.cxx		\
       ui.cxx		\
       libwavpack.cxx   \
       plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS}  ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${WAVPACK_CFLAGS} -I../..
CXXFLAGS += ${PLUGIN_CFLAGS}
LDFLAGS += -lstdc++
LIBS += ${WAVPACK_LIBS} ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS}
