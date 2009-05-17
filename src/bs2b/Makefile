PLUGIN = bs2b${PLUGIN_SUFFIX}

SRCS = plugin.c			\
       core/bs2btypes.h		\
       core/bs2bversion.h	\
       core/bs2b.h		\
       core/bs2b.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} -I../.. -I./core
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${MOWGLI_LIBS}
