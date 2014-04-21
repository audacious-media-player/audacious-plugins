include ../../extra.mk

PLUGIN = sndfile${PLUGIN_SUFFIX}

SRCS = plugin.cc

include ../../buildsys.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${SNDFILE_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${SNDFILE_LIBS} -lm
