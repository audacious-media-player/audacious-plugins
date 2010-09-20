PLUGIN = blur_scope${PLUGIN_SUFFIX}

SRCS = blur_scope.c	\
       config.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += -std=c99 ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} -I../..
LIBS += ${GTK_LIBS}
