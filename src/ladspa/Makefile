PLUGIN = ladspa${PLUGIN_SUFFIX}

SRCS = effect.c \
       loaded-list.c \
       plugin.c \
       plugin-list.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GTK_CFLAGS} ${GMODULE_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${GTK_LIBS} ${GMODULE_LIBS}
