PLUGIN = jackout${PLUGIN_SUFFIX}
SRCS = jack.c		\
       configure.c	\
       bio2jack.c

plugindir = audacious/${OUTPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${MOWGLI_CFLAGS} ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${SAMPLERATE_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${JACK_LIBS} ${SAMPLERATE_LIBS} -pthread
