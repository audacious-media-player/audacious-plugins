PLUGIN = mixer${PLUGIN_SUFFIX}

SRCS = mixer.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GLIB_LIBS}
