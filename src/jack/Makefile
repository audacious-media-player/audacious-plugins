PLUGIN = jackout${PLUGIN_SUFFIX}

SRCS = jack.c		\
       bio2jack.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${JACK_LIBS} -lsamplerate -lm
