PLUGIN = sdlout${PLUGIN_SUFFIX}

SRCS = sdlout.c \
       plugin.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS += -I../.. ${SDL_CFLAGS}
CFLAGS += ${GLIB_CFLAGS} ${PLUGIN_CFLAGS}
LIBS += -lm ${GLIB_LIBS} ${SDL_LIBS}
