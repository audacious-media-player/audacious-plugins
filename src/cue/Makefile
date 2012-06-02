PLUGIN = cue${PLUGIN_SUFFIX}

SRCS = cue.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS += -I../.. ${PLUGIN_CPPFLAGS} ${CUE_CFLAGS} ${GLIB_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${CUE_LIBS} ${GLIB_LIBS}
