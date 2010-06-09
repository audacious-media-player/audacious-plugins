PLUGIN = roar${PLUGIN_SUFFIX}

SRCS = roar.c gui.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += -lroar ${GTK_LIBS} ${GLIB_LIBS}
