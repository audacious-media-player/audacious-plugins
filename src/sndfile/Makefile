include ../../extra.mk

PLUGIN = sndfile${PLUGIN_SUFFIX}

SRCS = plugin.c

include ../../buildsys.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}   -I../..
LIBS += ${SNDFILE_LIBS} ${GTK_LIBS}
