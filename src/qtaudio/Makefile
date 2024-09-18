PLUGIN = qtaudio${PLUGIN_SUFFIX}

SRCS = qtaudio.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. ${QT_CFLAGS} ${QTMULTIMEDIA_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${QT_LIBS} ${QTMULTIMEDIA_LIBS}
