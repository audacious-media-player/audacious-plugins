PLUGIN = jackout${PLUGIN_SUFFIX}

SRCS = jack.c		\
       bio2jack.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += ${JACK_LIBS} -lsamplerate -lm
