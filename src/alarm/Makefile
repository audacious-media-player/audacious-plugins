PLUGIN = alarm${PLUGIN_SUFFIX}

SRCS = alarm.c		\
       interface.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PTHREAD_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PTHREAD_LIBS}
