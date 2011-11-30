include ../../extra.mk

PLUGIN = sndfile${PLUGIN_SUFFIX}

SRCS = plugin.c

include ../../buildsys.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}   -I../..
LIBS += ${SNDFILE_LIBS} ${GTK_LIBS} ${GLIB_LIBS} -lm
