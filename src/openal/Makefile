PLUGIN = openal${PLUGIN_SUFFIX}

SRCS = openal.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS} ${PTHREAD_CFLAGS} ${GTK_CFLAGS} ${OPENAL_CFLAGS}
CPPFLAGS += -I../..
LIBS += ${PTHREAD_LIBS} ${GTK_LIBS} ${OPENAL_LIBS}
