include ../../extra.mk

PLUGIN = vorbis$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = configure.c  fileinfo.c  vcedit.c  vorbis.c

include ../../buildsys.mk

LIBS += $(OGG_VORBIS_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(OGG_VORBIS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..
