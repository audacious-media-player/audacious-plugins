PLUGIN = alsa${PLUGIN_SUFFIX}

SRCS = alsa.c \
       config.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${GTK_CFLAGS} ${ALSA_CFLAGS}
CPPFLAGS += -I../..
LIBS += ${GTK_LIBS} ${ALSA_LIBS}
