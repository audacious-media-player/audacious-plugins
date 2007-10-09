PLUGIN = spectrum${PLUGIN_SUFFIX}
SRCS = spectrum.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${VISUALIZATION_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
