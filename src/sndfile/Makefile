include ../../extra.mk

PLUGIN = sndfile${PLUGIN_SUFFIX}

SRCS = plugin.c

include ../../buildsys.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${SNDFILE_CFLAGS} -I../..
LIBS += ${SNDFILE_LIBS} -lm
