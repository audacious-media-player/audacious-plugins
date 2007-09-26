PLUGIN = sndstretch${PLUGIN_SUFFIX}
SRCS = sndstretch_xmms.c sndstretch.c

plugindir = audacious/${EFFECT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${MOWGLI_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
