PLUGIN = amidi-plug${PLUGIN_SUFFIX}

SRCS = amidi-plug.cc		\
       backend-fluidsynth/b-fluidsynth.cc \
       i_midi.cc			\
       i_configure.cc		\
       i_configure-fluidsynth.cc	\
       i_fileinfo.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${FLUIDSYNTH_CFLAGS} -I../..
LIBS += ${FLUIDSYNTH_LIBS} -lm

ifeq ($(USE_GTK),yes)
CPPFLAGS += ${GTK_CFLAGS}
LIBS += ${GTK_LIBS}
endif
