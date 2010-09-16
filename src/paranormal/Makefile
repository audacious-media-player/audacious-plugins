PLUGIN = paranormal${PLUGIN_SUFFIX}

SRCS = actuators.c		\
       beatdetect.c		\
       builtins.c		\
       cfg.c			\
       cmaps.c			\
       containers.c		\
       drawing.c		\
       freq.c			\
       general.c		\
       misc.c			\
       paranormal.c		\
       plugin.c			\
       presets.c		\
       wave.c			\
       xform.c			\
       libcalc/dict.c		\
       libcalc/execute.c	\
       libcalc/function.c	\
       libcalc/parser.c		\
       libcalc/storage.c

SUBDIRS = presets

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${ARCH_DEFINES} ${XML_CFLAGS} ${SDL_CFLAGS} -I../.. -I.
LIBS += -lm ${GTK_LIBS} ${GLIB_LIBS} ${XML_LIBS} ${SDL_LIBS}
