PLUGIN = amidi-plug${PLUGIN_SUFFIX}

SRCS = amidi-plug.c		\
       i_midi.c			\
       i_backend.c		\
       i_configure.c		\
       i_configure-ap.c		\
       i_configure-fluidsynth.c	\
       i_utils.c		\
       i_fileinfo.c

SUBDIRS = backend-fluidsynth

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GMODULE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GMODULE_LIBS}

CPPFLAGS += -DAMIDIPLUGBACKENDDIR=\"${plugindir}/amidi-plug\" \
            -DAMIDIPLUG_FLUIDSYNTH
