PLUGIN = waveout${PLUGIN_SUFFIX}

SRCS = waveout.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${MOWGLI_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} -lwinmm
