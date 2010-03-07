PLUGIN = crossfade${PLUGIN_SUFFIX}

SRCS = callbacks.c			\
       cfgutil.c			\
       configure.c			\
       crossfade.c			\
       interface-2.0.c			\
       player.c				\
       support-2.0.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ALSA_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${ALSA_LIBS}
