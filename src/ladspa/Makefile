PLUGIN = ladspa${PLUGIN_SUFFIX}

SRCS = loaded-list.c \
       plugin.c \
       plugin-list.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += -I../..
CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS}
LIBS += -lm
