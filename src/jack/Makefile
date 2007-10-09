PLUGIN = jackout${PLUGIN_SUFFIX}
SRCS = jack.c		\
       configure.c	\
       bio2jack.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${SAMPLERATE_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${JACK_LIBS} ${SAMPLERATE_LIBS} -pthread
