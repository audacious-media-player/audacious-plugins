PLUGIN = sdlout${PLUGIN_SUFFIX}

SRCS = sdlout.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${PTHREAD_CFLAGS} ${GTK_CFLAGS} ${SDL_CFLAGS}
CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS}
LIBS += -lm ${PTHREAD_LIBS} ${GTK_LIBS} ${SDL_LIBS}
