include ../../extra.mk

PLUGIN = cdaudio-ng$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = cdaudio-ng.c configure.c

include ../../buildsys.mk

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) -Wall
CPPFLAGS = $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(CDIO_CFLAGS) $(CDDB_CFLAGS) -I../..
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(CDIO_LIBS) $(CDDB_LIBS)

