PLUGIN = pulse_audio${PLUGIN_SUFFIX}

SRCS = pulse_audio.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += -lpulse ${GTK_LIBS} ${GLIB_LIBS}
