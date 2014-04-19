PLUGIN = cue${PLUGIN_SUFFIX}

SRCS = cue.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS += -I../.. ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${CUE_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS} ${CUE_LIBS}
