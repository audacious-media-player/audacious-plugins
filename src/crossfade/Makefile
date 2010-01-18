PLUGIN = crossfade${PLUGIN_SUFFIX}

SRCS = callbacks.c			\
       cfgutil.c			\
       configure.c			\
       crossfade.c			\
       debug.c				\
       interface-2.0.c			\
       player.c				\
       support-2.0.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ALSA_CFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${ALSA_LIBS} -lpthread
