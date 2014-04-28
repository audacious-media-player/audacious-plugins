PLUGIN = ladspa${PLUGIN_SUFFIX}

SRCS = effect.cc \
       loaded-list.cc \
       plugin.cc \
       plugin-list.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${EFFECT_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${GTK_CFLAGS} ${GMODULE_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${GTK_LIBS} ${GMODULE_LIBS} -laudgui
