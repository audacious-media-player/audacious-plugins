PLUGIN = projectm${PLUGIN_SUFFIX}

SRCS = main.c		\
       video_init.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} ${SDL_CFLAGS} ${LIBPROJECTM_CFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${SDL_LIBS} ${LIBPROJECTM_LIBS} -lprojectM
