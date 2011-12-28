PLUGIN = alsa${PLUGIN_SUFFIX}

SRCS = alsa.c \
       config.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PTHREAD_CFLAGS} ${GTK_CFLAGS} ${ALSA_CFLAGS} -I../..
LIBS += ${PTHREAD_LIBS} ${GTK_LIBS} ${ALSA_LIBS}
