PLUGIN = song_change${PLUGIN_SUFFIX}

SRCS = song_change.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
