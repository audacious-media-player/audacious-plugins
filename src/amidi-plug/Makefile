PLUGIN = amidi-plug${PLUGIN_SUFFIX}

SRCS = amidi-plug.c		\
       i_vfslayer.c		\
       i_midi.c			\
       i_backend.c		\
       i_configure.c		\
       i_configure-ap.c		\
       i_configure-alsa.c	\
       i_configure-fluidsynth.c	\
       i_utils.c		\
       i_fileinfo.c

SUBDIRS = pcfg ${AMIDIPLUG_BACKENDS}

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${GMODULE_CFLAGS} ${MOWGLI_CFLAGS} \
	-DAMIDIPLUGBACKENDDIR=\"${plugindir}/${AMIDIPLUG_BACKEND_DIR}\" ${PTHREAD_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${GMODULE_LIBS} ${PTHREAD_LIBS} ./pcfg/libpcfg.a
