PLUGIN = kanashi${PLUGIN_SUFFIX}

SRCS = beatdetect.c		\
       cfg.c			\
       cmaps.c			\
       drawing.c		\
       freq.c			\
       general.c		\
       jsglue.c			\
       kanashi.c		\
       misc.c			\
       plugin.c			\
       wave.c			\
       xform.c			\

SUBDIRS = examples

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${ARCH_DEFINES} ${SDL_CFLAGS} -I../.. -I. -DPRESET_PATH="\"${prefix}/share/audacious/kanashi/presets\""
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${SDL_LIBS} -lmozjs
