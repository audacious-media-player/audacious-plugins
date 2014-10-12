PLUGIN = qtaudio${PLUGIN_SUFFIX}

SRCS = qtaudio.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. ${QTMULTIMEDIA_CFLAGS}
CXXFLAGS += ${GLIB_CFLAGS} ${PLUGIN_CFLAGS}
LIBS += -lm ${GLIB_LIBS} ${QTMULTIMEDIA_LIBS}
