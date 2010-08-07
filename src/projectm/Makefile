PLUGIN = projectm${PLUGIN_SUFFIX}

SRCS = main.c		\
       video_init.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${ARCH_DEFINES} ${SDL_CFLAGS} ${LIBPROJECTM_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${SDL_LIBS} ${LIBPROJECTM_LIBS} -lprojectM
