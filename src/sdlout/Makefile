PLUGIN = sdlout${PLUGIN_SUFFIX}

SRCS = sdlout.cc \
       plugin.cc \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. ${SDL_CFLAGS}
CXXFLAGS += ${GLIB_CFLAGS} ${PLUGIN_CFLAGS}
LIBS += -lm ${GLIB_LIBS} ${SDL_LIBS}
