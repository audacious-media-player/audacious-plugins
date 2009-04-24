PLUGIN = crossfade${PLUGIN_SUFFIX}

SRCS = callbacks.c			\
       cfgutil.c			\
       configure.c			\
       convert.c			\
       crossfade.c			\
       debug.c				\
       format.c				\
       interface-2.0.c			\
       monitor.c			\
       player.c				\
       support-2.0.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ALSA_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${ALSA_LIBS}
