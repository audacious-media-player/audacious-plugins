PLUGIN = mixer${PLUGIN_SUFFIX}

SRCS = mixer.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
CFLAGS += ${PLUGIN_CFLAGS}
