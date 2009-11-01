PLUGIN = alsa-gapless${PLUGIN_SUFFIX}

SRCS = alsa.c \
       config.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += -I../.. ${PLUGIN_CFLAGS} ${GLIB_CFLAGS} ${GTK_CFLAGS} ${ALSA_CFLAGS}
LIBS += ${GLIB_LIBS} ${GTK_LIBS} ${ALSA_LIBS}
