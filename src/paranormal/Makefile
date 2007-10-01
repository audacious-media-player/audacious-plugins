SUBDIRS = presets

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

plugindir = audacious/${VISUALIZATION_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} ${SDL_CFLAGS} -I../../intl -I../.. -I.
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${XML_LIBS} ${SDL_LIBS}
