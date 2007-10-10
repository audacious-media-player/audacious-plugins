PLUGIN = metronom${PLUGIN_SUFFIX}

SRCS = metronom.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} -I../../intl -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
