PLUGIN = alsa-gapless${PLUGIN_SUFFIX}

SRCS = alsa.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${GLIB_CFLAGS} ${GTK_CFLAGS} ${ALSA_CFLAGS}
LIBS += ${GLIB_LIBS} ${GTK_LIBS} ${ALSA_LIBS}
