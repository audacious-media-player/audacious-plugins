PLUGIN = song_change${PLUGIN_SUFFIX}

SRCS = formatter.c song_change.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${BEEP_DEFINES} ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS}
