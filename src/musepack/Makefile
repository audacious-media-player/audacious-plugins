PLUGIN = musepack${PLUGIN_SUFFIX}

SRCS = libmpc.cxx

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${TAGLIB_CFLAGS} -I../..
LIBS += ${TAGLIB_LIBS} ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS} -lmpcdec
