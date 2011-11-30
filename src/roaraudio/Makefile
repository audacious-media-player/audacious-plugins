PLUGIN = roar${PLUGIN_SUFFIX}

SRCS = roar.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} -std=gnu99
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${LIBROAR_CFLAGS} -I../..
LIBS += ${LIBROAR_LIBS} ${GTK_LIBS} ${GLIB_LIBS}
