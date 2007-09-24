PLUGIN = pulse_audio${PLUGIN_SUFFIX}
SRCS = pulse_audio.c

plugindir = audacious/${OUTPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lpulse ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
