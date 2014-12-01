PLUGIN = sdlout${PLUGIN_SUFFIX}

SRCS = sdlout.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. ${SDL_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${SDL_LIBS}
