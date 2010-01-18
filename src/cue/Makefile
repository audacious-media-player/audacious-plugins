PLUGIN = cue${PLUGIN_SUFFIX}

SRCS = cue.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS += ${CUE_CFLAGS} ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../.. -Wall
LIBS += ${GTK_LIBS} ${XML_LIBS} ${CUE_LIBS}
