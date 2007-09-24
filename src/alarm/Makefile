PLUGIN = alarm${PLUGIN_SUFFIX}
SRCS = alarm.c interface.c

plugindir = audacious/${GENERAL_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBADD = ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
