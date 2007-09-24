include ../../extra.mk

PLUGIN = filewriter$(SHARED_SUFFIX)
plugindir = audacious/$(OUTPUT_PLUGIN_DIR)

SRCS = filewriter.c wav.c mp3.c vorbis.c flac.c

include ../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GLIB_CFLAGS) $(GTK_CFLAGS) $(PANGO_CFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) -I../../intl -I../.. -Wall
LIBS += $(GTK_LIBS) ${FILEWRITER_LIBS}
