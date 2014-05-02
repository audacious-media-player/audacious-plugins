PLUGIN = amidi-plug${PLUGIN_SUFFIX}

SRCS = amidi-plug.cc		\
       backend-fluidsynth/b-fluidsynth.cc \
       i_midi.cc			\
       i_configure.cc		\
       i_configure-fluidsynth.cc	\
       i_utils.cc		\
       i_fileinfo.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${FLUIDSYNTH_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${FLUIDSYNTH_LIBS} -lm
