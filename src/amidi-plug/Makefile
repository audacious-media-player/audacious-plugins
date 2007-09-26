PLUGIN = amidi-plug$(PLUGIN_SUFFIX)
SUBDIRS = pcfg $(AMIDIPLUG_BACKENDS)

noinst_HEADERS = amidi-plug.h i_backend.h i_midi.h i_midievent.h \
                 i_vfslayer.h i_configure.h i_configure_private.h \
                 i_configure_file.h i_configure-ap.h i_configure-alsa.h \
                 i_configure-fluidsynth.h i_configure-dummy.h \
                 i_fileinfo.h i_utils.h i_common.h \
                 amidi-plug-icon.xpm amidi-plug.logo.xpm \
                 amidi-plug.midiicon.xpm

plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = amidi-plug.c i_vfslayer.c i_midi.c i_backend.c \
          i_configure.c i_configure-ap.c i_configure-alsa.c \
          i_configure-fluidsynth.c i_configure-dummy.c \
          i_utils.c i_fileinfo.c

include ../../buildsys.mk
include ../../extra.mk

OBJECTS = ${SOURCES:.c=.o}

EXT_DEPS = ./pcfg/libpcfg.a

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(GMODULE_CFLAGS) $(MOWGLI_CFLAGS) \
	-DAMIDIPLUGBACKENDDIR=\"$(AMIDIPLUGBACKENDDIR)\" -I../../intl -I../..

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(GMODULE_LIBS) -lpthread ./pcfg/libpcfg.a
