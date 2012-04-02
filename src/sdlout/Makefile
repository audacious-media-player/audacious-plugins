PLUGIN = sdlout${PLUGIN_SUFFIX}

SRCS = sdlout.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GTK_CFLAGS} ${SDL_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${GTK_LIBS} ${SDL_LIBS}
