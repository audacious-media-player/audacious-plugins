PLUGIN = ALSA${PLUGIN_SUFFIX}

SRCS = alsa-core.c \
       alsa-configure.c \
       alsa-ringbuffer.c \
       alsa-util.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ALSA_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${ALSA_LIBS}
