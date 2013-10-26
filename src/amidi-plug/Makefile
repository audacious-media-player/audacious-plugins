PLUGIN = amidi-plug${PLUGIN_SUFFIX}

SRCS = amidi-plug.c		\
       backend-fluidsynth/b-fluidsynth.c \
       i_midi.c			\
       i_configure.c		\
       i_configure-fluidsynth.c	\
       i_utils.c		\
       i_fileinfo.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${FLUIDSYNTH_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${FLUIDSYNTH_LIBS} -lm
