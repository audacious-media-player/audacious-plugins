PLUGIN = sdlout${PLUGIN_SUFFIX}

SRCS = sdlout.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS} ${PTHREAD_CFLAGS} ${GTK_CFLAGS} ${SDL_CFLAGS}
CPPFLAGS += -I../..
LIBS += ${PTHREAD_LIBS} ${GTK_LIBS} ${SDL_LIBS}
