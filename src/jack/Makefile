PLUGIN = jackout${PLUGIN_SUFFIX}

SRCS = jack.c		\
       bio2jack.c
#       configure.c	\

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${SAMPLERATE_CFLAGS} ${PTHREAD_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${JACK_LIBS} ${SAMPLERATE_LIBS} ${PTHREAD_LIBS} -lm
