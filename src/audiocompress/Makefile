PLUGIN = audiocompress${PLUGIN_SUFFIX}
SRCS = compress.c	\
       audacious-glue.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${EFFECT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
