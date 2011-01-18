PLUGIN = ap-alsa${PLUGIN_SUFFIX}

SRCS = b-alsa.c

include ../../../buildsys.mk
include ../../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}/${AMIDIPLUG_BACKEND_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${ALSA_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${ALSA_CFLAGS} ${GLIB_CFLAGS} -I../../..
LIBS += ${ALSA_LIBS} ../pcfg/libpcfg.a ${GLIB_LIBS}
