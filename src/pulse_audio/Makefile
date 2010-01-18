PLUGIN = pulse_audio${PLUGIN_SUFFIX}

SRCS = pulse_audio.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += ${BEEP_DEFINES} ${GTK_CFLAGS}  -I../..
LIBS += -lpulse ${GTK_LIBS}
