include ../../extra.mk

PLUGIN = libwav$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = wav$(WAV_SNDFILE).c

include ../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -Wall
LIBS += -L/usr/local/lib $(MOWGLI_LIBS) $(SNDFILE_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

