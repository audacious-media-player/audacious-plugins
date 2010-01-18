PLUGIN = jackout${PLUGIN_SUFFIX}

SRCS = jack.c		\
       configure.c	\
       bio2jack.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}  ${SAMPLERATE_CFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${JACK_LIBS} ${SAMPLERATE_LIBS}
