PLUGIN = alsa-gapless${PLUGIN_SUFFIX}

SRCS = alsa.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${GLIB_CFLAGS} ${ALSA_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${ALSA_LIBS}
