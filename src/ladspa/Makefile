PLUGIN = ladspa${PLUGIN_SUFFIX}
SRCS = ladspa.c

plugindir = audacious/${EFFECT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBADD = ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
