PLUGIN = cue${PLUGIN_SUFFIX}

SRCS = cue.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${CUE_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../.. -Wall
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${XML_LIBS} ${CUE_LIBS}
