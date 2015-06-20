PLUGIN = vtx${PLUGIN_SUFFIX}

SRCS = ay8912.cc		\
       info.cc		\
       lh5dec.cc		\
       vtx.cc		\
       vtxfile.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..

ifeq ($(USE_GTK),yes)
CPPFLAGS += ${GTK_CFLAGS}
LIBS += ${GTK_LIBS} -laudgui
endif
