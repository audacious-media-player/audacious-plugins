PLUGIN = wavpack${PLUGIN_SUFFIX}

SRCS = wavpack.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${WAVPACK_CFLAGS} -I../..
LIBS += ${WAVPACK_LIBS} ${GTK_LIBS} ${GLIB_LIBS}
