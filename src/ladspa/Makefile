PLUGIN = ladspa${PLUGIN_SUFFIX}

SRCS = effect.c \
       loaded-list.c \
       plugin.c \
       plugin-list.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${PTHREAD_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${PTHREAD_LIBS}
