PLUGIN = ap-alsa${PLUGIN_SUFFIX}

SRCS = b-alsa.c

include ../../../buildsys.mk
include ../../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}/${AMIDIPLUG_BACKEND_DIR}

CFLAGS += ${ALSA_CFLAGS}
CPPFLAGS += ${ALSA_CFLAGS} -I../../..
LIBS += ${ALSA_LIBS} ../pcfg/libpcfg.a
