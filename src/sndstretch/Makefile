PLUGIN = sndstretch${PLUGIN_SUFFIX}
SRCS = sndstretch_xmms.c sndstretch.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${MOWGLI_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
